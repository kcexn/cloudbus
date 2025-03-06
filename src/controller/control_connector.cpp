/*     
*   Copyright 2025 Kevin Exton
*   This file is part of Cloudbus.
*
*   Cloudbus is free software: you can redistribute it and/or modify it under the 
*   terms of the GNU Affero General Public License as published by the Free Software 
*   Foundation, either version 3 of the License, or any later version.
*
*   Cloudbus is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
*   without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
*   See the GNU Affero General Public License for more details.
*
*   You should have received a copy of the GNU Affero General Public License along with Cloudbus. 
*   If not, see <https://www.gnu.org/licenses/>. 
*/
#include "control_connector.hpp"
#include <tuple>
#include <sys/un.h>
#include <fcntl.h>
#ifdef PROFILE
    #include <chrono>
    #include <iostream>
#endif
namespace cloudbus {
    namespace controller {
        static constexpr std::streamsize MAX_BUFSIZE = 65536; //64KB
        static int set_flags(int fd){
            int flags = 0;
            if(fcntl(fd, F_SETFD, FD_CLOEXEC))
                throw std::runtime_error("Unable to set the cloexec flag.");
            if((flags = fcntl(fd, F_GETFL)) == -1)
                throw std::runtime_error("Unable to get file flags.");
            if(fcntl(fd, F_SETFL, flags | O_NONBLOCK))
                throw std::runtime_error("Unable to set the socket to nonblocking mode.");
            return fd;
        }
        static int _accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen){
            int fd =0;
            if((fd = accept(sockfd, addr, addrlen)) < 0){
                switch(errno){
                    case EINTR:
                        return _accept(sockfd, addr, addrlen);
                    case EWOULDBLOCK:
//                  case EAGAIN:
                        return -1;
                    default:
                        throw std::runtime_error("Unable to accept connected socket.");
                }
            } else return set_flags(fd);
        }
        static void state_update(control_connector::connection_type& conn, const messages::msgtype& type, const control_connector::connection_type::time_point time){
            switch(conn.state){
                case control_connector::connection_type::HALF_OPEN:
                    conn.timestamps[++conn.state] = time;
                case control_connector::connection_type::OPEN:
                case control_connector::connection_type::HALF_CLOSED:
                    if(type.op != messages::STOP) return;
                    conn.timestamps[++conn.state] = time;
                default: return;
            }
        }
        static std::array<char, 256> _buf = {};
        static std::ostream& stream_write(std::ostream& os, std::istream& is){
            while(auto gcount = is.readsome(_buf.data(), _buf.max_size()))
                if(os.write(_buf.data(), gcount).bad()) return os;
            return os;
        }              
        static std::ostream& stream_write(std::ostream& os, std::istream& is, std::streamsize maxlen){
            while(auto gcount = is.readsome(_buf.data(), std::min(maxlen, static_cast<std::streamsize>(_buf.max_size())))){
                if(os.write(_buf.data(), gcount).bad()) 
                    return os;
                maxlen -= gcount;
            }
            return os;
        }
        static control_connector::connections_type::iterator write_prepare(control_connector::connections_type& connections, const control_connector::north_type::stream_ptr& np, messages::msgheader& head, const std::streamsize& tellp){
            const std::streamsize pos = MAX_BUFSIZE-(tellp+sizeof(head));
            auto conn = connections.begin();
            while(conn < connections.end()){
                if(auto n = conn->north.lock(); n && n==np){
                    head.eid = conn->uuid;
                    if(auto s = conn->south.lock()){
                        if(s->tellp() >= pos)
                            s->flush();
                        if(s->tellp() > pos)
                            return conn;
                        ++conn;
                    } else conn = connections.erase(conn);
                } else ++conn;
            }
            return conn;
        }
        static std::size_t write_commit(control_connector::connections_type& connections, const control_connector::north_type::stream_ptr& np, const messages::msgheader& head, control_connector::trigger_type& triggers, std::istream& is, const std::streamsize& tellp){
            const auto time = control_connector::connection_type::clock_type::now();
            std::size_t connected = 0;
            for(auto& c: connections){
                if(auto n = c.north.lock(); n && n==np){
                    if(auto s = c.south.lock()){
                        if(is.tellg() == 0) state_update(c, head.type, time);
                        ++connected;
                        s->write(reinterpret_cast<const char*>(&head), sizeof(head));
                        stream_write(*s, is.seekg(0), tellp);
                        triggers.set(s->native_handle(), POLLOUT);
                    }
                }
            }
            return connected;
        }
        static int clear_triggers(int sockfd, control_connector::trigger_type& triggers, control_connector::event_mask& revents, const control_connector::event_mask& mask){
            revents &= ~mask;
            triggers.clear(sockfd, mask);
            return 0;
        }
        static control_connector::events_type::iterator read_restart(const int& sockfd, control_connector::trigger_type& triggers, control_connector::events_type& events, const control_connector::events_type::iterator& ev){
            triggers.set(sockfd, POLLIN);
            auto it = std::find_if(events.begin(), events.end(), [&](auto& e){ return e.fd == sockfd; });
            if(it == events.end()){
                auto off = ev - events.begin();
                events.push_back(control_connector::event_type{sockfd, POLLIN, POLLIN});
                return events.begin() + off;
            } else it->revents |= POLLIN;
            return ev;
        }
        control_connector::control_connector(trigger_type& triggers): Base(triggers){}
        control_connector::norths_type::iterator control_connector::make(norths_type& n, const north_type::address_type& addr, north_type::size_type addrlen){
            constexpr int backlog = 128;
            n.push_back(make_north(addr, addrlen));
            auto& s = n.back()->make(addr->sa_family, SOCK_STREAM, 0);
            auto sockfd = set_flags(std::get<north_type::native_handle_type>(s));
            int reuse = 1;
            setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
            if(bind(sockfd, addr, addrlen)) throw std::runtime_error("bind()");
            if(listen(sockfd, backlog)) throw std::runtime_error("listen()");
            triggers().set(sockfd, POLLIN);
            return --n.end();
        }
        control_connector::souths_type::iterator control_connector::make(souths_type& s, const south_type::address_type& addr, south_type::size_type addrlen){
            s.push_back(make_south(addr, addrlen));
            return --s.end();
        }
        control_connector::size_type control_connector::_handle(events_type& events){
            size_type handled = 0;
            for(auto ev = events.begin(); ev < events.end(); ++ev){
                if(ev->revents){
                    auto nit = std::find_if(north().begin(), north().end(), [&](auto& interface){
                        for(auto& stream: interface->streams()){
                            auto&[sockfd, nsp] = stream;
                            if(sockfd == ev->fd){
                                if(ev->revents & (POLLOUT | POLLERR))
                                    for(auto& c: connections())
                                        if(auto n = c.north.lock(); n && n==nsp)
                                            if(auto s = c.south.lock(); s && !s->eof())
                                                ev = read_restart(s->native_handle(), triggers(), events, ev);
                                handled += _handle(interface, stream, ev->revents);
                                return true;
                            }
                        }
                        return false;
                    });
                    if(nit != north().end()) continue;             
                    auto sit = std::find_if(south().begin(), south().end(), [&](auto& interface){
                        for(auto& stream: interface->streams()){
                            auto&[sockfd, ssp] = stream;
                            if(sockfd == ev->fd){
                                if(ev->revents & (POLLOUT | POLLERR))
                                    for(auto& c: connections())
                                        if(auto s = c.south.lock(); s && s==ssp)
                                            if(auto n = c.north.lock(); n && !n->eof())
                                                ev = read_restart(n->native_handle(), triggers(), events, ev);
                                handled += _handle(interface, stream, ev->revents);       
                                return true;
                            }
                        }
                        return false;
                    });
                    if(sit == south().end()){
                        triggers().clear(ev->fd);
                        ev->revents = 0;
                    }
                }
            }
            return handled;
        }
        int control_connector::_route(marshaller_type::north_format& buf, const shared_north& interface, north_type::stream_type& stream, event_mask& revents){
            auto&[nfd, nsp] = stream;
            const auto eof = nsp->eof();
            if(const auto p = buf.tellp(); eof || p > 0){
                messages::msgheader head = {
                    {},{1, static_cast<std::uint16_t>(static_cast<std::uint16_t>(p)+sizeof(head))},
                    {0,0},{(eof) ? messages::STOP : messages::DATA, 0}
                };
                if(write_prepare(connections(), nsp, head, p) != connections().end())
                    return clear_triggers(nfd, triggers(), revents, (POLLIN | POLLHUP));
                if(!write_commit(connections(), nsp, head, triggers(), buf, p)){
                    if(eof) return -1;
                    else if(!_north_connect_handler(interface, nsp, buf))
                        return clear_triggers(nfd, triggers(), revents, (POLLIN | POLLHUP));
                }
            }
            if(eof) triggers().clear(nfd, POLLIN);
            return 0;
        }
        int control_connector::_route(marshaller_type::south_format& buf, const shared_south& interface, south_type::stream_type& stream, event_mask& revents){
            constexpr std::streamsize HDRLEN = sizeof(messages::msgheader);
            auto&[sfd, ssp] = stream;
            const auto *eid = buf.eid();
            if(const auto *type = eid != nullptr ? buf.type() : nullptr; type != nullptr){
                const std::streamsize seekpos = 
                    (buf.tellg() <= HDRLEN)
                    ? HDRLEN
                    : static_cast<std::streamsize>(buf.tellg());
                const std::streamsize pos = buf.tellp();
                for(auto conn = connections().begin(); conn < connections().end();){
                    if(auto s = conn->south.lock(); conn->uuid == *eid && s && s == ssp){
                        if(auto n = conn->north.lock()){
                            buf.seekg(seekpos);
                            triggers().set(n->native_handle(), POLLOUT);
                            if(pos > seekpos && !_south_write(n, buf))
                                return clear_triggers(sfd, triggers(), revents, (POLLIN | POLLHUP));
                            if(!buf.eof() && buf.tellg()==buf.len()->length)
                                buf.setstate(std::ios_base::eofbit);
                            if(seekpos == HDRLEN) 
                                state_update(*conn, *type, connection_type::clock_type::now());
                            return 0;
                        } else conn = connections().erase(conn);
                    } else ++conn;
                }
                if(!buf.eof() && pos==buf.len()->length)
                    buf.setstate(std::ios_base::eofbit);
            }
            if(ssp->eof()) return -1;
            return 0;
        }
        void control_connector::_north_err_handler(shared_north& interface, const north_type::stream_type& stream, event_mask& revents){
            messages::msgheader head = {
                {}, {1, static_cast<std::uint16_t>(sizeof(head))},
                {0,0},{messages::STOP, 0}
            };
            const auto time = connection_type::clock_type::now();
            auto&[nfd, nsp] = stream;
            for(auto conn = connections().begin(); conn < connections().end();){
                if(auto n = conn->north.lock(); n && n == nsp){
                    if(auto s = conn->south.lock()){
                        triggers().set(s->native_handle(), POLLOUT);
                        if(conn->state < connection_type::HALF_CLOSED || (!n->eof() && conn->state == connection_type::HALF_CLOSED)){
                            head.eid = conn->uuid;
                            s->write(reinterpret_cast<const char*>(&head), sizeof(head));
                        }
                        state_update(*conn, head.type, time);
                        if(conn->state == connection_type::CLOSED)
                            conn = connections().erase(conn);
                        else ++conn;
                    } else conn = connections().erase(conn);
                } else ++conn;
            }            
            revents = 0;          
            triggers().clear(nfd);
            interface->erase(stream);
        }
        std::streamsize control_connector::_north_write(south_type::stream_ptr& ssp, const connection_type& conn, marshaller_type::north_format& buf){
            std::streamsize bp = buf.tellp(), size=sizeof(messages::msgheader)+bp, pos=MAX_BUFSIZE-size;
            if(ssp->fail()) return -1;
            if(ssp->tellp() >= pos)
                if(ssp->flush().bad()) return -1;
            if(ssp->tellp() < pos){
                auto nsp = conn.north.lock();
                const messages::msgheader head = {
                    conn.uuid,
                    {1, static_cast<std::uint16_t>(size)},
                    {0,0},
                    {(!nsp || nsp->eof()) ? messages::STOP : messages::DATA, 0}
                };
                if(ssp->write(reinterpret_cast<const char*>(&head), sizeof(head)).bad()) return -1;
                if(stream_write(*ssp, buf.seekg(0), bp).bad()) return -1;
                return size;
            } else return 0;
        }
        std::streamsize control_connector::_north_connect_handler(const shared_north& interface, north_type::stream_ptr& nsp, marshaller_type::north_format& buf){
            auto posit = std::find(north().cbegin(), north().cend(), interface);
            auto& sbd = south()[posit - north().cbegin()];
            const auto n = connection_type::clock_type::now();
            const auto eid = messages::make_uuid_v4();
            auto empty = sbd->streams().empty();
            auto&[sfd, ssp] = (empty) ? sbd->make(sbd->address()->sa_family, SOCK_STREAM, 0) : sbd->streams().back();
            if(empty){
                set_flags(sfd);
                ssp->connectto(sbd->address(), sbd->addrlen());
            }
            connections().push_back(
                (nsp->eof())
                ? connection_type{eid, nsp, ssp, connection_type::HALF_CLOSED, {n,n,n,{}}}
                : connection_type{eid, nsp, ssp, connection_type::HALF_OPEN, {n,{},{},{}}}
            );
            triggers().set(sfd, (POLLIN | POLLOUT));
            return _north_write(ssp, connections().back(), buf);
        }
        int control_connector::_north_pollin_handler(const shared_north& interface, north_type::stream_type& stream, event_mask& revents){
            auto it = marshaller().unmarshal(stream);
            if(std::get<north_type::stream_ptr>(stream)->gcount() == 0)
                revents &= ~(POLLIN | POLLHUP);
            return route(std::get<marshaller_type::north_format>(*it), interface, stream, revents);
        }
        int control_connector::_north_accept_handler(shared_north& interface, const north_type::stream_type& stream, event_mask& revents){
            int sockfd = 0;
            const auto listenfd = std::get<north_type::native_handle_type>(stream);
            while((sockfd = _accept(listenfd, nullptr, nullptr)) >= 0){
                interface->make(sockfd);
                triggers().set(sockfd, POLLIN);
            }
            revents &= ~(POLLIN | POLLHUP);
            return 0;
        }
        void control_connector::_north_state_handler(shared_north& interface, const north_type::stream_type& stream, event_mask& revents){
            const auto&[nfd, nsp] = stream;
            for(auto conn = connections().begin(); conn < connections().end();){
                if(auto n = conn->north.lock(); n && n == nsp){
                    switch(conn->state){
                        case connection_type::CLOSED:
                            if(auto s = conn->south.lock())
                                triggers().set(s->native_handle(), POLLOUT);
                            connections().erase(conn);
                            return _north_err_handler(interface, stream, revents);
                        case connection_type::HALF_CLOSED:
                            revents |= POLLHUP;
                            shutdown(nfd, SHUT_WR);
                        default: return;
                    }
                } else ++conn;
            }
        }
        int control_connector::_north_pollout_handler(north_type::stream_type& stream, event_mask& revents){
            auto&[nfd, nsp]=stream;
            nsp->flush();
            if(nsp->fail() || revents & (POLLERR | POLLNVAL))
                return -1;
            if(nsp->tellp() == 0)
                triggers().clear(nfd, POLLOUT);
            revents &= ~(POLLOUT | POLLERR | POLLNVAL);
            return 0;
        }
        control_connector::size_type control_connector::_handle(shared_north& interface, north_type::stream_type& stream, event_mask& revents){
            size_type handled = 0;
            if(revents & (POLLOUT | POLLERR | POLLNVAL)){
                ++handled;
                if(_north_pollout_handler(stream, revents))
                    _north_err_handler(interface, stream, revents);
                else _north_state_handler(interface, stream, revents);
            }
            if(revents & (POLLIN | POLLHUP)){
                ++handled;
                if(stream == interface->streams().front()) {
                    if(_north_accept_handler(interface, stream, revents))
                        _north_err_handler(interface, stream, revents);
                } else if(_north_pollin_handler(interface, stream, revents))
                    _north_err_handler(interface, stream, revents);
            }
            return handled;
        }
        void control_connector::_south_err_handler(shared_south& interface, const south_type::stream_type& stream, event_mask& revents){
            const auto&[sfd, ssp] = stream;
            for(auto conn = connections().begin(); conn < connections().end();){
                if(auto s = conn->south.lock(); s && s == ssp){
                    if(auto n = conn->north.lock()){
                        state_update(*conn, {messages::STOP, 0}, connection_type::clock_type::now());
                        triggers().set(n->native_handle(), POLLOUT);
                        ++conn;
                    } else conn = connections().erase(conn);
                } else ++conn;
            }
            revents = 0;
            triggers().clear(sfd);
            interface->erase(stream);
        }
        std::streamsize control_connector::_south_write(north_type::stream_ptr& n, marshaller_type::south_format& buf){
            std::streamsize g=buf.tellg(), p=buf.tellp(), pos=MAX_BUFSIZE-(p-g);
            if(n->fail()) return -1;
            if(n->tellp() >= pos)
                if(n->flush().bad())
                    return -1;
            if(n->tellp() < pos){
                if(stream_write(*n, buf).bad())
                    return -1;
                return p-g;
            } else return 0;
        }
        int control_connector::_south_pollin_handler(const shared_south& interface, south_type::stream_type& stream, event_mask& revents){
            auto it = marshaller().marshal(stream);
            if(std::get<south_type::stream_ptr>(stream)->gcount() == 0)
                revents &= ~(POLLIN | POLLHUP);
            return route(std::get<marshaller_type::south_format>(*it), interface, stream, revents);
        }
        int control_connector::_south_state_handler(const south_type::stream_type& stream){
            std::size_t count = 0;
            const auto& ssp = std::get<south_type::stream_ptr>(stream);
            for(auto conn = connections().begin(); conn < connections().end();){
                if(auto s = conn->south.lock()){
                    if(s == ssp){
                        if(conn->state != connection_type::CLOSED){
                            ++count; ++conn;
                        } else conn = connections().erase(conn);
                    } else ++conn;
                } else conn = connections().erase(conn);
            }
            if(count == 0) return -1;
            return 0;
        }
        int control_connector::_south_pollout_handler(south_type::stream_type& stream, event_mask& revents){
            auto&[sfd, ssp] = stream;
            ssp->flush();
            if(ssp->fail() || revents & (POLLERR | POLLNVAL))
                return -1;
            if(ssp->tellp() == 0)
                triggers().clear(sfd, POLLOUT);
            revents &= ~(POLLOUT | POLLERR | POLLNVAL);
            return 0;
        }
        control_connector::size_type control_connector::_handle(shared_south& interface, south_type::stream_type& stream, event_mask& revents){
            size_type handled = 0;
            if(revents & (POLLOUT | POLLERR | POLLNVAL)){
                ++handled;
                if(_south_pollout_handler(stream, revents))
                    _south_err_handler(interface, stream, revents);
                else if(_south_state_handler(stream))
                    _south_err_handler(interface, stream, revents);
            }
            if(revents & (POLLIN | POLLHUP)){
                ++handled;
                if(_south_pollin_handler(interface, stream, revents))
                    _south_err_handler(interface, stream, revents);
            }
            return handled;
        }
    }
}