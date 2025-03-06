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
#include "proxy_connector.hpp"
#include <sys/un.h>
#include <fcntl.h>
namespace cloudbus{
    namespace proxy {
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
            if((fd = accept(sockfd, addr, addrlen)) >= 0){
                return set_flags(fd);
            } else {
                switch(errno){
                    case EINTR:
                        return _accept(sockfd, addr, addrlen);
                    case EWOULDBLOCK:
//                  case EAGAIN:
                        return -1;
                    default:
                        throw std::runtime_error("Unable to accept connected socket.");
                }
            }
        }    
        static void state_update(proxy_connector::connection_type& conn, const messages::msgtype& type, const proxy_connector::connection_type::time_point time){
            switch(conn.state){
                case proxy_connector::connection_type::HALF_OPEN:
                    conn.timestamps[++conn.state] = time;
                case proxy_connector::connection_type::OPEN:
                case proxy_connector::connection_type::HALF_CLOSED:
                    if(type.op != messages::STOP) return;
                    conn.timestamps[++conn.state] = time;
                default: return;
            }
        }
        static std::array<char, 256> _buf = {};         
        static std::streamsize stream_write(std::ostream& os, std::istream& is, std::streamsize len){
            std::streamsize pos=MAX_BUFSIZE-len;
            if(os.fail()) return -1;
            if(os.tellp() >= pos && os.flush().bad())
                return -1;
            if(os.tellp() < pos){
                std::streamsize count = 0;
                std::streamsize size = std::min(len, static_cast<std::streamsize>(_buf.max_size()));
                while(auto gcount = is.readsome(_buf.data(), size)){
                    if(os.write(_buf.data(), gcount).bad()) 
                        return -1;
                    count += gcount;
                    size = std::min(len-count, static_cast<std::streamsize>(_buf.max_size()));
                }
                return count;
            } else return 0;
        }
        static proxy_connector::connections_type::iterator write_prepare(proxy_connector::connections_type& connections, const messages::uuid& uuid, const std::streamsize& len){
            const std::streamsize pos = MAX_BUFSIZE-len;
            auto conn = connections.begin();
            while(conn < connections.end()){
                if(conn->uuid == uuid){
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
        static std::size_t write_commit(proxy_connector::connections_type& connections, const messages::uuid& uuid, const messages::msgtype& type, proxy_connector::trigger_type& triggers, std::istream& is, const std::streamsize& len){
            const auto time = proxy_connector::connection_type::clock_type::now();
            const auto seekpos = is.tellg();
            std::size_t connected = 0;
            for(auto& c: connections){
                if(auto s = c.south.lock(); c.uuid == uuid && s){
                    if(seekpos == 0) state_update(c, type, time);
                    ++connected;
                    stream_write(*s, is.seekg(seekpos), len);
                    triggers.set(s->native_handle(), POLLOUT);
                }
            }
            return connected;
        }
        static int clear_triggers(int sockfd, proxy_connector::trigger_type& triggers, proxy_connector::event_mask& revents, const proxy_connector::event_mask& mask){
            revents &= ~mask;
            triggers.clear(sockfd, mask);
            return 0;
        }
        static proxy_connector::events_type::iterator read_restart(const int& sockfd, proxy_connector::trigger_type& triggers, proxy_connector::events_type& events, const proxy_connector::events_type::iterator& ev){
            triggers.set(sockfd, POLLIN);
            auto it = std::find_if(events.begin(), events.end(), [&](auto& e){ return e.fd == sockfd; });
            if(it == events.end()){
                auto off = ev - events.begin();
                events.push_back(proxy_connector::event_type{sockfd, POLLIN, POLLIN});
                return events.begin() + off;
            } else it->revents |= POLLIN;
            return ev;
        }

        proxy_connector::proxy_connector(trigger_type& triggers): Base(triggers){}
        proxy_connector::norths_type::iterator proxy_connector::make(norths_type& n, const north_type::address_type& addr, north_type::size_type addrlen){
            constexpr int backlog = 128;
            n.push_back(make_north(addr, addrlen));
            auto& interface = n.back();
            auto& stream = interface->make(interface->address()->sa_family, SOCK_STREAM, 0);
            auto sockfd = set_flags(std::get<north_type::native_handle_type>(stream));
            if(bind(sockfd, interface->address(), interface->addrlen())) throw std::runtime_error("bind()");
            if(listen(sockfd, backlog)) throw std::runtime_error("listen()");
            triggers().set(sockfd, POLLIN);
            return --n.end();
        }
        proxy_connector::souths_type::iterator proxy_connector::make(souths_type& s, const south_type::address_type& addr, south_type::size_type addrlen){
            s.push_back(make_south(addr, addrlen));
            return --s.end();
        }
        proxy_connector::size_type proxy_connector::_handle(events_type& events){
            size_type handled = 0;
            for(auto ev=events.begin(); ev < events.end(); ++ev){
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
        int proxy_connector::_route(marshaller_type::north_format& buf, const shared_north& interface, north_type::stream_type& stream, event_mask& revents){
            auto&[nfd, nsp] = stream;
            const auto *eid = buf.eid();
            if(const auto *type = eid != nullptr ? buf.type() : nullptr; type != nullptr){
                const auto seekpos = buf.tellg();
                if(write_prepare(connections(), *eid, buf.tellp()-seekpos) != connections().end())
                    return clear_triggers(nfd, triggers(), revents, (POLLIN | POLLHUP));
                if(write_commit(connections(), *eid, *type, triggers(), buf.seekg(seekpos), buf.tellp()-seekpos)){
                    if(!buf.eof() && buf.tellg()==buf.len()->length)
                        buf.setstate(std::ios_base::eofbit);
                    if(auto rem=buf.len()->length-buf.tellp(); nsp->eof() && rem){
                        std::vector<char> tmp(rem);
                        for(auto& c: connections()){
                            if(auto s = c.south.lock(); c.uuid == *eid && s){
                                triggers().set(s->native_handle(), POLLOUT);
                                s->write(tmp.data(), tmp.size());
                            }
                        }
                    }
                } 
                else if(nsp->eof()) return -1;
                else if(!_north_connect_handler(interface, nsp, buf))
                    return clear_triggers(nfd, triggers(), revents, (POLLIN | POLLHUP));
            }
            if(nsp->eof()) return -1;
            return 0;
        }
        int proxy_connector::_route(marshaller_type::south_format& buf, const shared_south& interface, south_type::stream_type& stream, event_mask& revents){
            auto&[sfd, ssp] = stream;
            const auto *eid = buf.eid();
            if(const auto *type = eid != nullptr ? buf.type() : nullptr; type != nullptr){
                const auto seekpos = buf.tellg(), pos = buf.tellp();
                for(auto conn = connections().begin(); conn < connections().end();){
                    if(auto s = conn->south.lock(); conn->uuid == *eid && s && s==ssp){
                        if(auto n = conn->north.lock()){
                            if(auto len = pos-seekpos){
                                triggers().set(n->native_handle(), POLLOUT);
                                if(!stream_write(*n, buf.seekg(seekpos), len))
                                    return clear_triggers(sfd, triggers(), revents, (POLLIN | POLLHUP));
                                if(!buf.eof() && buf.tellg()==buf.len()->length)
                                    buf.setstate(std::ios_base::eofbit);
                            }
                            if(seekpos == 0) state_update(*conn, *type, connection_type::clock_type::now());
                            if(auto rem = buf.len()->length-buf.tellp(); ssp->eof() && rem) {
                                triggers().set(n->native_handle(), POLLOUT);
                                std::vector<char> tmp(rem);
                                n->write(tmp.data(), tmp.size());
                            }
                            return 0;
                        } else conn = connections().erase(conn);
                    } else ++conn;
                }
                if(pos == buf.len()->length)
                    buf.setstate(std::ios_base::eofbit);
            }
            if(ssp->eof()) return -1;
            return 0;
        }
        void proxy_connector::_north_err_handler(shared_north& interface, const north_type::stream_type& stream, event_mask& revents){
            messages::msgheader head = {
                {}, {1, sizeof(head)},
                {0,0}, {messages::STOP, 0}
            };
            auto&[nfd, nsp] = stream;
            const auto time = connection_type::clock_type::now();
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
        std::streamsize proxy_connector::_north_connect_handler(const shared_north& interface, const north_type::stream_ptr& nsp, marshaller_type::north_format& buf){
            auto posit = std::find(north().cbegin(), north().cend(), interface);
            auto& sbd = south()[posit - north().cbegin()];
            const auto n = connection_type::clock_type::now();
            auto empty = sbd->streams().empty();
            auto&[sfd, ssp] = (empty) ? sbd->make(sbd->address()->sa_family, SOCK_STREAM, 0) : sbd->streams().back();
            if(empty){
                set_flags(sfd);
                ssp->connectto(sbd->address(), sbd->addrlen());
            }
            connections().push_back(
                (buf.type()->op==messages::STOP)
                ? connection_type{*buf.eid(), nsp, ssp, connection_type::HALF_CLOSED, {n,n,n,{}}}
                : connection_type{*buf.eid(), nsp, ssp, connection_type::HALF_OPEN, {n,{},{},{}}}
            );
            triggers().set(sfd, (POLLIN | POLLOUT));
            return stream_write(*ssp, buf, buf.tellp()-buf.tellg());
        }
        int proxy_connector::_north_pollin_handler(const shared_north& interface, north_type::stream_type& stream, event_mask& revents){
            auto it = marshaller().unmarshal(stream);
            if(std::get<north_type::stream_ptr>(stream)->gcount() == 0)
                revents &= ~(POLLIN | POLLHUP);
            return route(std::get<marshaller_type::north_format>(*it), interface, stream, revents);
        }
        int proxy_connector::_north_accept_handler(shared_north& interface, const north_type::stream_type& stream, event_mask& revents){
            int sockfd = 0;
            const auto listenfd = std::get<north_type::native_handle_type>(stream);
            while((sockfd = _accept(listenfd, nullptr, nullptr)) >= 0){
                interface->make(sockfd);
                triggers().set(sockfd, POLLIN);
            }
            revents &= ~(POLLIN | POLLHUP);
            return 0;
        }
        void proxy_connector::_north_state_handler(shared_north& interface, const north_type::stream_type& stream, event_mask& revents){
            const auto& nsp = std::get<north_type::stream_ptr>(stream);
            for(auto conn = connections().begin(); conn < connections().end();){
                if(auto n = conn->north.lock(); n && n == nsp){
                    if(conn->state == connection_type::CLOSED){
                        if(auto s = conn->south.lock())
                            triggers().set(s->native_handle(), POLLOUT);
                        conn = connections().erase(conn);
                    } else ++conn;
                } else ++conn;
            }
        }
        int proxy_connector::_north_pollout_handler(north_type::stream_type& stream, event_mask& revents){
            auto&[nfd, nsp] = stream;
            nsp->flush();
            if(nsp->fail() || revents & (POLLERR | POLLNVAL))
                return -1;
            if(nsp->tellp() == 0)
                triggers().clear(nfd, POLLOUT);
            revents &= ~(POLLOUT | POLLERR | POLLNVAL);
            return 0;
        }
        proxy_connector::size_type proxy_connector::_handle(shared_north& interface, north_type::stream_type& stream, event_mask& revents){
            size_type handled = 0;
            if(revents & (POLLOUT | POLLERR | POLLNVAL)){
                ++handled;
                if(_north_pollout_handler(stream, revents))
                    _north_err_handler(interface, stream, revents);
                else _north_state_handler(interface, stream, revents);
            }
            if(revents & (POLLIN | POLLHUP)){
                ++handled;
                if(stream == interface->streams().front()){
                    if(_north_accept_handler(interface, stream, revents))
                        _north_err_handler(interface, stream, revents);
                } else if(_north_pollin_handler(interface, stream, revents))
                    _north_err_handler(interface, stream, revents);
            }
            return handled;
        }

        void proxy_connector::_south_err_handler(shared_south& interface, const south_type::stream_type& stream, event_mask& revents){
            messages::msgheader head = {
                {}, {1, sizeof(head)},
                {0,0}, {messages::STOP, 0}
            };
            auto&[sfd, ssp] = stream;
            const auto time = connection_type::clock_type::now();
            for(auto conn = connections().begin(); conn < connections().end();){
                if(auto s = conn->south.lock(); s && s == ssp){
                    head.eid = conn->uuid;
                    if(auto n = conn->north.lock()){
                        triggers().set(n->native_handle(), POLLOUT);
                        if(conn->state < connection_type::HALF_CLOSED || (!s->eof() && conn->state == connection_type::HALF_CLOSED))
                            n->write(reinterpret_cast<const char*>(&head), sizeof(head));
                        state_update(*conn, head.type, time);
                        if(conn->state == connection_type::CLOSED)
                            conn = connections().erase(conn);
                        else ++conn;
                    } conn = connections().erase(conn);
                } else ++conn;
            }
            revents = 0;
            triggers().clear(sfd);
            interface->erase(stream);
        }
        int proxy_connector::_south_pollin_handler(const shared_south& interface, south_type::stream_type& stream, event_mask& revents){       
            auto it = marshaller().marshal(stream);
            if(std::get<south_type::stream_ptr>(stream)->gcount() == 0)
                revents &= ~(POLLIN | POLLHUP);
            return route(std::get<marshaller_type::south_format>(*it), interface, stream, revents);
        }
        int proxy_connector::_south_state_handler(const south_type::stream_type& stream){
            std::size_t count = 0;
            const auto& ssp = std::get<south_type::stream_ptr>(stream);
            for(auto conn = connections().begin(); conn < connections().end();){
                if(auto s = conn->south.lock(); s && s == ssp){
                    if(conn->state != connection_type::CLOSED){
                        ++count; ++conn;
                    } else conn = connections().erase(conn);
                } else ++conn;
            }
            if(count == 0) return -1;
            return 0;
        }
        int proxy_connector::_south_pollout_handler(south_type::stream_type& stream, event_mask& revents){
            auto&[sfd, ssp] = stream;
            ssp->flush();
            if(ssp->fail() || revents & (POLLERR | POLLNVAL))
                return -1;
            if(ssp->tellp() == 0)
                triggers().clear(sfd, POLLOUT);
            revents &= ~(POLLOUT | POLLERR | POLLNVAL);
            return 0;
        }
        proxy_connector::size_type proxy_connector::_handle(shared_south& interface, south_type::stream_type& stream, event_mask& revents){
          size_type handled = 0;
          if(revents & (POLLOUT | POLLERR | POLLNVAL)){
            ++handled;
            if(_south_pollout_handler(stream, revents))
                _south_err_handler(interface, stream, revents);
            else if (_south_state_handler(stream))
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