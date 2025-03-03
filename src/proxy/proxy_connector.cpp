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
            if(os.fail()) return 0;
            if(os.tellp() >= pos)
                if(os.flush().bad())
                    return 0;
            if(os.tellp() < pos){
                std::streamsize count = 0;
                std::streamsize size = std::min(len, static_cast<std::streamsize>(_buf.max_size()));
                while(auto gcount = is.readsome(_buf.data(), size)){
                    if(os.write(_buf.data(), gcount).bad()) 
                        return 0;
                    count += gcount;
                    size = std::min(len-count, static_cast<std::streamsize>(_buf.max_size()));
                }
                return count;
            }
            return 0;
        }

        proxy_connector::proxy_connector(trigger_type& triggers): Base(triggers){}
        proxy_connector::norths_type::iterator proxy_connector::make(norths_type& n, const north_type::address_type addr, north_type::size_type addrlen){
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
        proxy_connector::souths_type::iterator proxy_connector::make(souths_type& s, const south_type::address_type addr, south_type::size_type addrlen){
            s.push_back(make_south(addr, addrlen));
            return --s.end();
        }
        int proxy_connector::_handle(events_type& events){
            int handled = 0;
            for(auto ev=events.begin(); ev < events.end(); ++ev){
                if(auto& revents = ev->revents){
                    auto nit = std::find_if(north().begin(), north().end(), [&](auto& interface){
                        for(auto& stream: interface->streams()){
                            if(std::get<north_type::native_handle_type>(stream) == ev->fd){
                                handled += _handle(interface, stream, revents);
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
                                if(revents & POLLOUT && !ssp->tellp()){
                                    for(auto& c: connections()){
                                        if(auto s = c.south.lock(); s && s == ssp &&
                                                c.state == connection_type::HALF_OPEN){
                                            /* connect() has resolved. */
                                            if(auto n = c.north.lock()){
                                                auto nfd = n->native_handle();
                                                triggers().set(nfd, POLLIN);
                                                auto it = std::find_if(events.begin(), events.end(), [&](auto& e){ return e.fd == nfd; });
                                                if(it == events.end()){
                                                    handled += _handle(interface, stream, revents);
                                                    auto off = ev - events.begin();
                                                    events.push_back(event_type{nfd, POLLIN, POLLIN});
                                                    ev = events.begin() + off;
                                                    return true;
                                                } else it->revents |= POLLIN;
                                            }
                                        }
                                    }
                                }                           
                                handled += _handle(interface, stream, revents);
                                return true;
                            }
                        }
                        return false;
                    });
                    if(sit == south().end()){
                        triggers().clear(ev->fd);
                        revents = 0;
                    }
                }
            }
            return handled;
        }

        void proxy_connector::_north_err_handler(shared_north& interface, const north_type::stream_type& stream, event_mask& revents){
            revents = 0;   
            for(auto conn = connections().begin(); conn < connections().end();){
                if(auto n = conn->north.lock()){
                    if(n == std::get<north_type::stream_ptr>(stream)){
                        const messages::msgheader head = {
                            conn->uuid,
                            {1, sizeof(head)},
                            {0,0},
                            {messages::STOP, 0}
                        };
                        state_update(*conn, head.type, connection_type::clock_type::now());
                        switch(conn->state){
                            case connection_type::HALF_CLOSED:
                                if(auto s = conn->south.lock()){
                                    s->write(reinterpret_cast<const char*>(&head), sizeof(head));
                                    triggers().set(s->native_handle(), POLLOUT);
                                    ++conn;
                                    break;
                                }
                            default: conn = connections().erase(conn);
                        }
                    } else ++conn;
                } else conn = connections().erase(conn);
            }
            triggers().clear(std::get<north_type::native_handle_type>(stream));
            interface->erase(stream);
        }
        std::streamsize proxy_connector::_north_connect_handler(const shared_north& interface, const north_type::stream_ptr& nsp, marshaller_type::north_format& buf){
            auto posit = std::find(north().cbegin(), north().cend(), interface);
            auto& sbd = south()[posit - north().cbegin()];
            const auto n = connection_type::clock_type::now();
            if(sbd->streams().empty()){
                auto&[sockfd, ssp] = sbd->make(sbd->address()->sa_family, SOCK_STREAM, 0);
                set_flags(sockfd);
                ssp->connectto(sbd->address(), sbd->addrlen());
                triggers().set(sockfd, (POLLIN | POLLOUT));
                connections().push_back(
                    (buf.type()->op == messages::STOP)
                    ? connection_type{*buf.eid(), nsp, ssp, connection_type::HALF_CLOSED, {n,n,n,{}}}
                    : connection_type{*buf.eid(), nsp, ssp, connection_type::HALF_OPEN, {n,{},{},{}}}
                );
                return 0;
            } else {
                auto&[sockfd, ssp] = sbd->streams().back();
                connections().push_back(
                    (buf.type()->op == messages::STOP)
                    ? connection_type{*buf.eid(), nsp, ssp, connection_type::HALF_CLOSED, {n,n,n,{}}}
                    : connection_type{*buf.eid(), nsp, ssp, connection_type::HALF_OPEN, {n,{},{},{}}}
                );
                triggers().set(sockfd, POLLOUT);
                return stream_write(*ssp, buf, buf.tellp()-buf.tellg());
            }
        }
        int proxy_connector::_north_pollin_handler(const shared_north& interface, north_type::stream_type& stream, event_mask& revents){
            /* forward the data arriving on the northbound connection to the southbound service. */
            auto it = marshaller().unmarshal(stream);
            auto&[nfd, nsp] = stream;
            if(nsp->gcount() == 0)
                revents &= ~(POLLIN | POLLHUP);
            auto& buf = std::get<marshaller_type::north_format>(*it);
            const auto *eid = buf.eid();
            if(const auto *type = eid != nullptr ? buf.type() : nullptr; type != nullptr){
                const auto seekpos = buf.tellg();
                for(auto conn = connections().begin(); conn < connections().end();){
                    if(conn->uuid == *eid){
                        if(auto s = conn->south.lock()){
                            if(auto len = buf.tellp()-seekpos){
                                while(!stream_write(*s, buf.seekg(seekpos), len)){
                                    if(s->fail()) break;
                                    auto trig_ = trigger_type();
                                    trig_.set(s->native_handle(), POLLOUT);
                                    trig_.wait(std::chrono::milliseconds(-1));
                                }
                                if(!buf.eof() && buf.tellg()==buf.len()->length)
                                    buf.setstate(std::ios_base::eofbit);
                                triggers().set(s->native_handle(), POLLOUT);
                            }
                            state_update(*conn, *type, connection_type::clock_type::now());
                            ++conn;
                        } else {
                            buf.setstate(std::ios_base::eofbit);
                            conn = connections().erase(conn);
                        }
                    } else ++conn;
                }
                if(!buf.eof() && buf.tellp() != buf.tellg()){
                    if(!_north_connect_handler(interface, nsp, buf)){
                        revents &= ~(POLLIN | POLLHUP);
                        triggers().clear(nfd, POLLIN);
                    }
                }
            }
            if(nsp->eof()) return -1;
            return 0;
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
            for(auto conn = connections().begin(); conn < connections().end();){
                if(auto n = conn->north.lock()){
                    if(conn->state == connection_type::CLOSED){
                        if(auto s = conn->south.lock())
                            triggers().set(s->native_handle(), POLLOUT);
                        conn = connections().erase(conn);
                    } else ++conn;
                } else conn = connections().erase(conn);
            }
        }
        int proxy_connector::_north_pollout_handler(north_type::stream_type& stream, event_mask& revents){
            if(revents & POLLERR) return -1;
            auto& nsp = std::get<north_type::stream_ptr>(stream);
            if(nsp->fail()) return -1;
            if(nsp->flush().bad()) return -1;
            if(nsp->tellp() == 0)
                triggers().clear(std::get<north_type::native_handle_type>(stream), POLLOUT);
            revents &= ~(POLLOUT | POLLERR);
            return 0;
        }
        int proxy_connector::_handle(shared_north& interface, north_type::stream_type& stream, event_mask& revents){
            int handled = 0;
            if(revents & (POLLOUT | POLLERR)){
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
            revents = 0;
            for(auto conn = connections().begin(); conn < connections().end();){
                if(auto s = conn->south.lock()){
                    if(s == std::get<south_type::stream_ptr>(stream)){
                        const messages::msgheader head = {
                            conn->uuid,
                            {1, sizeof(head)},
                            {0,0},
                            {messages::STOP, 0}
                        };
                        state_update(*conn, head.type, connection_type::clock_type::now());
                        switch(conn->state){
                            case connection_type::HALF_CLOSED:
                                if(auto n = conn->north.lock()){
                                    n->write(reinterpret_cast<const char*>(&head), sizeof(head));
                                    triggers().set(n->native_handle(), POLLOUT);
                                    ++conn;
                                    break;
                                }
                            default: conn = connections().erase(conn);
                        }
                    } else ++conn;
                } else conn = connections().erase(conn);
            }
            triggers().clear(std::get<south_type::native_handle_type>(stream));
            interface->erase(stream);
            return;
        }
        int proxy_connector::_south_pollin_handler(const shared_south& interface, south_type::stream_type& stream, event_mask& revents){       
            /* forward the data arriving on the northbound connection to the southbound service. */
            auto it = marshaller().marshal(stream);
            auto& ssp = std::get<south_type::stream_ptr>(stream);
            if(ssp->gcount() == 0)
                revents &= ~(POLLIN | POLLHUP);
            auto& buf = std::get<marshaller_type::south_format>(*it);
            const auto *eid = buf.eid();
            if(const auto *type = eid != nullptr ? buf.type() : nullptr; type != nullptr){
                const auto seekpos = buf.tellg();
                for(auto conn = connections().begin(); conn < connections().end();){
                    if(conn->uuid == *eid){
                        if(auto n = conn->north.lock()){
                            if(auto len = buf.tellp()-seekpos){
                                while(!stream_write(*n, buf.seekg(seekpos), len)){
                                    if(n->fail()) break;
                                    auto trig_ = trigger_type();
                                    trig_.set(n->native_handle(), POLLOUT);
                                    trig_.wait(std::chrono::milliseconds(-1));
                                }
                                if(!buf.eof() && buf.tellg()==buf.len()->length)
                                    buf.setstate(std::ios_base::eofbit);
                                triggers().set(n->native_handle(), POLLOUT);
                            }                   
                            state_update(*conn, *type, connection_type::clock_type::now());
                            ++conn;
                        } else {
                            buf.setstate(std::ios_base::eofbit);
                            conn = connections().erase(conn);
                        }
                    } else ++conn;
                }
                if(!buf.eof() && buf.tellg() != buf.tellp())
                    buf.setstate(std::ios_base::eofbit);
            }
            if(ssp->eof()) return -1;
            return 0;
        }
        int proxy_connector::_south_state_handler(const south_type::stream_type& stream){
            std::size_t count = 0;
            for(auto conn = connections().begin(); conn < connections().end();){
                if(auto s = conn->south.lock()){
                    if(s == std::get<south_type::stream_ptr>(stream)){
                        if(conn->state != connection_type::CLOSED){
                            ++count; ++conn;
                        } else conn = connections().erase(conn);
                    } else ++conn;
                } else conn = connections().erase(conn);
            }
            if(count == 0) return -1;
            return 0;
        }
        int proxy_connector::_south_pollout_handler(south_type::stream_type& stream, event_mask& revents){
            if(revents & POLLERR) return -1;
            auto& ssp = std::get<south_type::stream_ptr>(stream);
            if(ssp->fail()) return -1;
            if(ssp->flush().bad()) return -1;
            if(ssp->tellp() == 0)
                triggers().clear(std::get<south_type::native_handle_type>(stream), POLLOUT);
            revents &= ~(POLLOUT | POLLERR);
            return 0;
        }
        int proxy_connector::_handle(shared_south& interface, south_type::stream_type& stream, event_mask& revents){
          int handled = 0;
          if(revents & (POLLOUT | POLLERR)){
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