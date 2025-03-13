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
#include "segment_connector.hpp"
#include <sys/un.h>
#include <fcntl.h>
#ifdef PROFILE
    #include <chrono>
    #include <iostream>
#endif
namespace cloudbus{
    namespace segment {
        static constexpr std::streamsize MAX_BUFSIZE = 65536 * 512; /* 32MiB */
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
            while(int fd = accept(sockfd, addr, addrlen)){
                if(fd < 0){
                    switch(errno){
                        case EINTR: continue;
                        case EWOULDBLOCK:
                        /* case EAGAIN: */
                            return -1;
                        default:
                            throw std::runtime_error("Unable to accept connected socket.");
                    }
                } else return set_flags(fd);
            }
            return 0;
        }
        static void state_update(
            segment_connector::connection_type& conn,
            const messages::msgtype& type,
            const segment_connector::connection_type::time_point time
        ){
            switch(conn.state){
                case segment_connector::connection_type::HALF_OPEN:
                    conn.timestamps[++conn.state] = time;
                case segment_connector::connection_type::OPEN:
                case segment_connector::connection_type::HALF_CLOSED:
                    if(type.op != messages::STOP) return;
                    conn.timestamps[++conn.state] = time;
                default: return;
            }
        }       
        static std::ostream& stream_write(std::ostream& os, std::istream& is){
            std::array<char, 256> _buf = {};
            while(auto gcount = is.readsome(_buf.data(), _buf.max_size()))
                if(os.write(_buf.data(), gcount).bad())
                    return os;
            return os;
        }
        static std::ostream& stream_write(std::ostream& os, std::istream& is, std::streamsize maxlen){
            std::array<char, 256> _buf = {};
            while(auto gcount = is.readsome(_buf.data(), std::min(maxlen, static_cast<std::streamsize>(_buf.max_size())))){
                if(os.write(_buf.data(), gcount).bad())
                    return os;
                maxlen -= gcount;
            }
            return os;
        }
        static int clear_triggers(
            int sockfd,
            segment_connector::trigger_type& triggers,
            segment_connector::event_mask& revents,
            const segment_connector::event_mask& mask
        ){
            revents &= ~mask;
            triggers.clear(sockfd, mask);
            return 0;
        }
        static segment_connector::events_type::iterator read_restart(
            const int& sockfd,
            segment_connector::trigger_type& triggers,
            segment_connector::events_type& events,
            const segment_connector::events_type::iterator& ev
        ){
            triggers.set(sockfd, POLLIN);
            auto it = std::find_if(events.begin(), events.end(), [&](auto& e){ return e.fd == sockfd; });
            if(it == events.end()){
                auto off = ev - events.begin();
                events.push_back(segment_connector::event_type{sockfd, POLLIN, POLLIN});
                return events.begin() + off;
            } else it->revents |= POLLIN;
            return ev;
        }

        segment_connector::segment_connector(trigger_type& triggers): Base(triggers){}
        segment_connector::norths_type::iterator segment_connector::make(norths_type& n, const registry::address_type& address){
            switch(address.index()){
                case registry::SOCKADDR:
                {
                    auto&[addr, addrlen, protocol] = std::get<registry::SOCKADDR>(address);
                    n.push_back(std::make_shared<north_type>(reinterpret_cast<const struct sockaddr*>(&addr), addrlen, protocol));
                    if(protocol == "TCP" || protocol == "UNIX"){
                        auto& hnd = n.back()->make(addr.ss_family, SOCK_STREAM, 0);
                        auto& sockfd = std::get<north_type::native_handle_type>(*hnd);
                        set_flags(sockfd);
                        int reuse = 1;
                        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
                        if(bind(sockfd, reinterpret_cast<const struct sockaddr*>(&addr), addrlen))
                            throw std::runtime_error("bind()");
                        if(listen(sockfd, 128))
                            throw std::runtime_error("listen()");
                        triggers().set(sockfd, POLLIN);
                    } else throw std::invalid_argument("Invalid configuration.");
                    break;
                }
                case registry::URL:
                {
                    auto&[host, protocol] = std::get<registry::URL>(address);
                    n.push_back(std::make_shared<north_type>(host, protocol));
                    break;
                }
                case registry::URN:
                    if(auto& urn = std::get<registry::URN>(address); !urn.empty()){
                        n.push_back(std::make_shared<north_type>(urn));
                        break;
                    }
                default:
                    throw std::invalid_argument("Invalid configuration.");
            }
            return --n.end();
        }
        segment_connector::souths_type::iterator segment_connector::make(souths_type& s, const registry::address_type& address){
            switch(address.index()){
                case registry::SOCKADDR:
                {
                    auto&[addr, addrlen, protocol] = std::get<registry::SOCKADDR>(address);
                    s.push_back(std::make_shared<south_type>(reinterpret_cast<const struct sockaddr*>(&addr), addrlen, protocol));
                    break;
                }
                case registry::URL:
                {
                    auto&[host, protocol] = std::get<registry::URL>(address);
                    s.push_back(std::make_shared<south_type>(host, protocol));
                    break;
                }
                case registry::URN:
                    if(auto& urn = std::get<registry::URN>(address); !urn.empty()){
                        s.push_back(std::make_shared<south_type>(urn));
                        break;
                    }
                default:
                    throw std::invalid_argument("Invalid configuration.");
            }            
            return --s.end();
        }
        segment_connector::size_type segment_connector::_handle(events_type& events){
          size_type handled = 0;
          for(auto ev = events.begin(); ev < events.end(); ++ev){
            if(ev->revents){
              auto nit = std::find_if(north().begin(), north().end(), [&](auto& interface){
                for(auto& stream: interface->streams()){
                    auto&[sockfd, nsp] = *stream;
                    if(sockfd == ev->fd){
                        if(ev->revents & (POLLOUT | POLLERR))
                            for(auto& c: connections())
                                if(auto n = c.north.lock(); n && n == nsp)
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
                    auto&[sockfd, ssp] = *stream;
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
        int segment_connector::_route(marshaller_type::north_format& buf, const shared_north& interface, const north_type::handle_ptr& stream, event_mask& revents){
            constexpr std::streamsize HDRLEN = sizeof(messages::msgheader);
            const auto&[nfd, nsp] = *stream;
            const auto *eid = buf.eid();
            if(const auto *type = eid != nullptr ? buf.type() : nullptr; type != nullptr){
                const std::streamsize seekpos = 
                    (buf.tellg() <= HDRLEN)
                        ? HDRLEN 
                        : static_cast<std::streamsize>(buf.tellg());
                const std::streamsize pos = buf.tellp();
                for(auto conn = connections().begin(); conn < connections().end(); ++conn){
                    if(auto n = conn->north.lock(); conn->uuid == *eid && n && n == nsp){
                        if(auto s = conn->south.lock()){
                            if(conn->state==connection_type::HALF_CLOSED && !s->eof()){
                                if(!buf.eof() && pos==buf.len()->length)
                                    buf.setstate(std::ios_base::eofbit);
                            } else {
                                buf.seekg(seekpos);
                                triggers().set(s->native_handle(), POLLOUT);
                                if(pos > seekpos && !_north_write(s, buf))
                                    return clear_triggers(nfd, triggers(), revents, (POLLIN | POLLHUP));
                                if(!buf.eof() && buf.tellg() == buf.len()->length)
                                    buf.setstate(std::ios_base::eofbit);
                                if(seekpos == HDRLEN)
                                    state_update(*conn, *type, connection_type::clock_type::now());
                            }
                        } else connections().erase(conn);
                        return 0;
                    }
                }
                if(!buf.eof() && buf.tellg() != pos){
                    buf.seekg(HDRLEN);
                    if(pos > HDRLEN && !north_connect(interface, nsp, buf))
                        return clear_triggers(nfd, triggers(), revents, (POLLIN | POLLHUP));
                }
            }
            if(nsp->eof()) return -1;
            return 0;
        }
        int segment_connector::_route(marshaller_type::south_format& buf, const shared_south& interface, const south_type::handle_ptr& stream, event_mask& revents){
            auto&[sfd, ssp] = *stream;
            const auto p = buf.tellp();
            if(const auto eof = ssp->eof(); eof || p > 0){
                for(auto conn = connections().begin(); conn < connections().end(); ++conn){
                    if(auto s = conn->south.lock(); s && s == ssp){
                        if(auto n = conn->north.lock()){
                            messages::msgtype t = {messages::DATA, 0};
                            if(eof) t.op = messages::STOP;
                            buf.seekg(0);
                            triggers().set(n->native_handle(), POLLOUT);
                            if(!_south_write(n, *conn, buf))
                                return clear_triggers(sfd, triggers(), revents, (POLLIN | POLLHUP));
                            state_update(*conn, t, connection_type::clock_type::now());
                            if(conn->state == connection_type::CLOSED){
                                connections().erase(conn);
                                return -1;
                            }
                            if(eof) triggers().clear(sfd, POLLIN);
                            return 0;
                        } else conn = --connections().erase(conn);
                    }
                }
                return -1;
            }
            if(p == 0) return 0;
            return -1;
        }
        std::streamsize segment_connector::_north_connect(const shared_north& interface, const north_type::stream_ptr& nsp, marshaller_type::north_format& buf){
            auto posit = std::find(north().cbegin(), north().cend(), interface);
            auto& sbd = south()[posit - north().cbegin()];
            auto& hnd = sbd->make();
            sbd->register_connect(hnd, [&triggers=triggers()](const auto& hnd, const auto *addr, auto addrlen){
                auto&[sfd, ssp] = *hnd;
                if(!sfd){
                    auto& sockfd = ssp->native_handle();
                    sockfd = socket(addr->sa_family, SOCK_STREAM, 0);
                    sfd = set_flags(sockfd);
                }
                ssp->connectto(addr, addrlen);
                triggers.set(sfd, (POLLIN | POLLOUT));
            });
            const auto n = connection_type::clock_type::now();
            auto& ssp = std::get<south_type::stream_ptr>(*hnd);
            connections().push_back(
                (buf.type()->op == messages::STOP)
                ? connection_type{*buf.eid(), nsp, ssp, connection_type::HALF_CLOSED, {n,n,n,{}}}
                : connection_type{*buf.eid(), nsp, ssp, connection_type::HALF_OPEN, {n,{},{},{}}}
            );
            return _north_write(ssp, buf);
        }
        void segment_connector::_north_err_handler(const shared_north& interface, const north_type::handle_ptr& stream, event_mask& revents){
            const auto time = connection_type::clock_type::now();
            const auto&[nfd, nsp] = *stream;
            for(auto conn = connections().begin(); conn < connections().end(); ++conn){
                if(auto n = conn->north.lock(); n && n == nsp){
                    if(auto s = conn->south.lock()){
                        state_update(*conn, {messages::STOP, 0}, time);
                        triggers().set(s->native_handle(), POLLOUT);
                    } else conn = --connections().erase(conn);
                }
            }
            revents = 0;
            triggers().clear(nfd);
            interface->erase(stream);
        }
        std::streamsize segment_connector::_north_write(const south_type::stream_ptr& s, marshaller_type::north_format& buf){
            std::streamsize g=buf.tellg(), p=buf.tellp(), pos=MAX_BUFSIZE-(p-g);
            if(s->tellp() >= pos)
                if(s->flush().bad())
                    return -1;
            if(s->tellp() < pos){
                if(stream_write(*s, buf).bad())
                    return -1;
                return p-g;
            } else return 0;
        }
        int segment_connector::_north_pollin_handler(const shared_north& interface, const north_type::handle_ptr& stream, event_mask& revents){
            auto it = marshaller().unmarshal(stream);
            if(std::get<north_type::stream_ptr>(*stream)->gcount() == 0)
                revents &= ~(POLLIN | POLLHUP);
            return route(std::get<marshaller_type::north_format>(*it), interface, stream, revents);
        }
        int segment_connector::_north_accept_handler(const shared_north& interface, const north_type::handle_ptr& stream, event_mask& revents){
            int sockfd = 0;
            const auto listenfd = std::get<north_type::native_handle_type>(*stream);
            while((sockfd = _accept(listenfd, nullptr, nullptr)) >= 0){
                interface->make(sockfd, true);
                triggers().set(sockfd, POLLIN);
            }
            revents &= ~(POLLIN | POLLHUP);
            return 0;
        }
        int segment_connector::_north_pollout_handler(const north_type::handle_ptr& stream, event_mask& revents){
            auto&[nfd, nsp] = *stream;
            nsp->flush();
            if(nsp->fail() || revents & (POLLERR | POLLNVAL))
                return -1;
            if(nsp->tellp() == 0)
                triggers().clear(nfd, POLLOUT);
            revents &= ~(POLLOUT | POLLERR | POLLNVAL);
            return 0;
        }
        segment_connector::size_type segment_connector::_handle(const shared_north& interface, const north_type::handle_ptr& stream, event_mask& revents){
            size_type handled = 0;
            if(revents & (POLLOUT | POLLERR | POLLNVAL)){
                ++handled;
                if(_north_pollout_handler(stream, revents))
                    _north_err_handler(interface, stream, revents);
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
        void segment_connector::_south_err_handler(const shared_south& interface, const south_type::handle_ptr& stream, event_mask& revents){
            const auto&[sfd, ssp] = *stream;
            for(auto conn = connections().begin(); conn < connections().end(); ++conn){
                if(auto s = conn->south.lock(); s && s == ssp){
                    if(auto n = conn->north.lock()){
                        const messages::msgheader head = {
                            conn->uuid, {1, sizeof(head)},
                            {0,0},{messages::STOP, 0}
                        };
                        if(conn->state < connection_type::HALF_CLOSED || (!s->eof() && conn->state == connection_type::HALF_CLOSED)){
                            n->write(reinterpret_cast<const char*>(&head), sizeof(head));
                            triggers().set(n->native_handle(), POLLOUT);
                        }
                        state_update(*conn, head.type, connection_type::clock_type::now());
                        if(conn->state == connection_type::CLOSED)
                            conn = --connections().erase(conn);
                        break;
                    } else conn = --connections().erase(conn);
                }
            }
            revents = 0;
            triggers().clear(sfd);
            interface->erase(stream);
        }
        std::streamsize segment_connector::_south_write(const north_type::stream_ptr& n, const connection_type& conn, marshaller_type::south_format& buf){
            std::streamsize p=buf.tellp(), size=sizeof(messages::msgheader)+p, pos=MAX_BUFSIZE-size;
            if(n->tellp() >= pos)
                if(n->flush().bad())
                    return -1;
            if(n->tellp() > pos)
                return 0;
            auto s = conn.south.lock();
            const messages::msgheader head = {
                conn.uuid, 
                {1, static_cast<std::uint16_t>(sizeof(head) + p)}, 
                {0,0},
                {(!s || s->eof()) ? messages::STOP : messages::DATA, 0}
            };
            if(n->write(reinterpret_cast<const char*>(&head), sizeof(head)).bad())
                return -1;
            if(stream_write(*n, buf, p).bad())
                return -1;
            return size;
        }
        int segment_connector::_south_pollin_handler(const shared_south& interface, const south_type::handle_ptr& stream, event_mask& revents){
            auto it = marshaller().marshal(stream);
            if(std::get<south_type::stream_ptr>(*stream)->gcount() == 0)
                revents &= ~(POLLIN | POLLHUP);
            return route(std::get<marshaller_type::south_format>(*it), interface, stream, revents);
        }
        void segment_connector::_south_state_handler(const shared_south& interface, const south_type::handle_ptr& stream, event_mask& revents){
            const auto&[sfd, ssp] = *stream;
            for(auto conn = connections().begin(); conn < connections().end(); ++conn){
                if(auto s = conn->south.lock(); s && s == ssp){
                    switch(conn->state){
                        case connection_type::CLOSED:
                            connections().erase(conn);
                            return _south_err_handler(interface, stream, revents);
                        case connection_type::HALF_CLOSED:
                            revents |= POLLHUP;
                            shutdown(sfd, SHUT_WR);
                        default: return;
                    }
                }
            }
        }
        int segment_connector::_south_pollout_handler(const south_type::handle_ptr& stream, event_mask& revents){
            auto&[sfd, ssp] = *stream;
            ssp->flush();
            if(ssp->fail() || revents & (POLLERR | POLLNVAL))
                return -1;
            if(ssp->tellp() == 0)
                triggers().clear(sfd, POLLOUT);
            revents &= ~(POLLOUT | POLLERR | POLLNVAL);
            return 0;
        }
        segment_connector::size_type segment_connector::_handle(const shared_south& interface, const south_type::handle_ptr& stream, event_mask& revents){
            size_type handled = 0;
            if(revents & (POLLOUT | POLLERR | POLLNVAL)){
                ++handled;
                if(_south_pollout_handler(stream, revents)){
                    _south_err_handler(interface, stream, revents);
                } else _south_state_handler(interface, stream, revents);
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