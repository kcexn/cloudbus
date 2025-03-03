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
        static void state_update(segment_connector::connection_type& conn, const messages::msgtype& type, const segment_connector::connection_type::time_point time){
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
        static std::array<char, 256> _buf = {};
        static std::ostream& stream_write(std::ostream& os, std::istream& is){
            while(auto gcount = is.readsome(_buf.data(), _buf.max_size()))
                if(os.write(_buf.data(), gcount).bad()) return os;
            return os;
        }              
        static std::ostream& stream_write(std::ostream& os, std::istream& is, std::streamsize maxlen){
            while(auto gcount = is.readsome(_buf.data(), std::min(maxlen, static_cast<std::streamsize>(_buf.max_size())))){
                if(os.write(_buf.data(), gcount).bad()) return os;
                maxlen -= gcount;
            }
            return os;
        }
        segment_connector::segment_connector(trigger_type& triggers): Base(triggers){}
        segment_connector::norths_type::iterator segment_connector::make(norths_type& n, const north_type::address_type addr, north_type::size_type addrlen){
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
        segment_connector::souths_type::iterator segment_connector::make(souths_type& s, const south_type::address_type addr, south_type::size_type addrlen){
            s.push_back(make_south(addr, addrlen));
            return --s.end();
        }

        int segment_connector::_handle(events_type& events){
          int handled = 0;
          for(auto ev = events.begin(); ev < events.end(); ++ev){
            if(ev->revents){
              auto nit = std::find_if(north().begin(), north().end(), [&](auto& interface){
                for(auto& stream: interface->streams()){
                    auto&[sockfd, nsp] = stream;
                    if(sockfd == ev->fd){
                        if(ev->revents & (POLLOUT | POLLERR)){
                            for(auto& c: connections()){
                                if(auto n = c.north.lock(); n && n == nsp){
                                    if(auto s = c.south.lock(); s && !s->eof()){
                                        auto sfd = s->native_handle();
                                        triggers().set(sfd, POLLIN);
                                        auto it = std::find_if(events.begin(), events.end(), [&](auto& e){ return e.fd == sfd; });
                                        if(it == events.end()){
                                            auto off = ev - events.begin();
                                            events.push_back(event_type{sfd, POLLIN, POLLIN});
                                            ev = events.begin()+off;
                                        } else it->revents |= POLLIN;                            
                                    }
                                }
                            }
                        }
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
                        if(ev->revents & (POLLOUT | POLLERR)){
                            for(auto& c: connections()){
                                if(auto s = c.south.lock(); s && s==ssp){
                                    if(auto n = c.north.lock(); n && !n->eof()){
                                        auto nfd = n->native_handle();
                                        triggers().set(nfd, POLLIN);
                                        auto it = std::find_if(events.begin(), events.end(), [&](auto& e){ return e.fd == nfd; });
                                        if(it == events.end()){
                                            auto off = ev - events.begin();
                                            events.push_back(event_type{nfd, POLLIN, POLLIN});
                                            ev = events.begin()+off;
                                        } else it->revents |= POLLIN;
                                    }
                                }
                            }
                        }
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

        void segment_connector::_north_err_handler(shared_north& interface, const north_type::stream_type& stream, event_mask& revents){
            revents = 0;
            for(auto conn = connections().begin(); conn < connections().end();){
                if(auto n = conn->north.lock()){
                    if(n == std::get<north_type::stream_ptr>(stream)){
                        state_update(*conn, {messages::STOP, 0}, connection_type::clock_type::now());
                        switch(conn->state) {
                            case connection_type::HALF_CLOSED:
                                if(auto s = conn->south.lock()){
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
        std::streamsize segment_connector::_north_write(south_type::stream_ptr& s, marshaller_type::north_format& buf){
            std::streamsize g=buf.tellg(), p=buf.tellp(), pos=MAX_BUFSIZE-(p-g);
            if(s->fail()) return -1;
            if(s->tellp() >= pos)
                if(s->flush().bad())
                    return -1;
            if(s->tellp() < pos){
                if(stream_write(*s, buf).bad())
                    return -1;
                return p-g;
            } else return -1;
        }
        std::streamsize segment_connector::_north_connect_handler(const shared_north& interface, north_type::stream_ptr& nsp, marshaller_type::north_format& buf){
            auto posit = std::find(north().cbegin(), north().cend(), interface);
            auto& sbd = south()[posit - north().cbegin()];
            auto&[sockfd, ssp] = sbd->make(sbd->address()->sa_family, SOCK_STREAM, 0);
            set_flags(sockfd);
            ssp->connectto(sbd->address(), sbd->addrlen());
            const auto n = connection_type::clock_type::now();
            connections().push_back(
                (buf.type()->op == messages::STOP)
                ? connection_type{*buf.eid(), nsp, ssp, connection_type::HALF_CLOSED, {n,n,n,{}}}
                : connection_type{*buf.eid(), nsp, ssp, connection_type::HALF_OPEN, {n,{},{},{}}}
            );
            triggers().set(sockfd, (POLLIN | POLLOUT));
            return _north_write(ssp, buf);
        }
        int segment_connector::_north_pollin_handler(const shared_north& interface, north_type::stream_type& stream, event_mask& revents){
            constexpr std::streamsize HDRLEN = sizeof(messages::msgheader);
            auto it = marshaller().unmarshal(stream);
            auto& buf = std::get<marshaller_type::north_format>(*it);
            auto&[nfd, nsp] = stream;
            if(nsp->gcount() == 0)
                revents &= ~(POLLIN | POLLHUP);
            const auto *eid = buf.eid();
            if(const auto *type = eid != nullptr ? buf.type() : nullptr; type != nullptr){
                const auto seekpos = buf.tellg() <= HDRLEN ? HDRLEN : static_cast<std::streamsize>(buf.tellg());
                for(auto conn = connections().begin(); conn < connections().end();){
                    if(conn->uuid == *eid){
                        if(auto s = conn->south.lock()){
                            buf.seekg(seekpos);
                            triggers().set(s->native_handle(), POLLOUT);
                            if(_north_write(s, buf) < 0){
                                revents &= ~(POLLIN | POLLHUP);
                                triggers().clear(nfd, POLLIN);
                                return 0;
                            }
                            state_update(*conn, *type, connection_type::clock_type::now());
                            ++conn;
                            if(nsp->eof()) return -1;
                            return 0;
                        } else {
                            buf.setstate(std::ios_base::eofbit);
                            conn = connections().erase(conn);
                        }
                    } else ++conn;
                }
                if(!buf.eof() && buf.tellg() != buf.tellp()){
                    buf.seekg(HDRLEN);
                    if(!_north_connect_handler(interface, nsp, buf)){
                        revents &= ~(POLLIN | POLLHUP);
                        triggers().clear(nfd, POLLIN);
                    }
                }
            }
            if(nsp->eof()) return -1;
            return 0;
        }
        int segment_connector::_north_accept_handler(shared_north& interface, const north_type::stream_type& stream, event_mask& revents){
            int sockfd = 0;
            const auto listenfd = std::get<north_type::native_handle_type>(stream);
            while((sockfd = _accept(listenfd, nullptr, nullptr)) >= 0){
                interface->make(sockfd);
                triggers().set(sockfd, POLLIN);
            }
            revents &= ~(POLLIN | POLLHUP);
            return 0;
        }
        int segment_connector::_north_pollout_handler(north_type::stream_type& stream, event_mask& revents){
            if(revents & POLLERR) return -1;
            auto& nsp = std::get<north_type::stream_ptr>(stream);
            if(nsp->fail()) return -1;
            if(nsp->flush().bad()) return -1;
            if(nsp->tellp() == 0)
                triggers().clear(std::get<north_type::native_handle_type>(stream), POLLOUT);
            revents &= ~(POLLOUT | POLLERR);
            return 0;
        }
        int segment_connector::_handle(shared_north& interface, north_type::stream_type& stream, event_mask& revents){
            int handled = 0;
            if(revents & (POLLOUT | POLLERR)){
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

        void segment_connector::_south_err_handler(shared_south& interface, const south_type::stream_type& stream, event_mask& revents){
            revents = 0;
            triggers().clear(std::get<south_type::native_handle_type>(stream));
            interface->erase(stream);
        }
        std::streamsize segment_connector::_south_write(north_type::stream_ptr& n, const connection_type& conn, marshaller_type::south_format& buf){
            std::streamsize p=buf.tellp(), size=sizeof(messages::msgheader)+p, pos=MAX_BUFSIZE-size;
            if(n->fail()) return 0;
            if(n->tellp() >= pos)
                if(n->flush().bad())
                    return 0;
            if(n->tellp() < pos){
                auto s = conn.south.lock();
                const messages::msgheader head = {
                    conn.uuid, 
                    {1, static_cast<std::uint16_t>(sizeof(head) + p)}, 
                    {0,0}, 
                    {(!s || s->eof()) ? messages::STOP : messages::DATA, 0}
                };
                if(n->write(reinterpret_cast<const char*>(&head), sizeof(head)).bad())
                    return 0;
                if(stream_write(*n, buf, p).bad())
                    return 0;
                return size;
            } else return 0;
        }
        int segment_connector::_south_pollin_handler(shared_south& interface, south_type::stream_type& stream, event_mask& revents){
            /* forward the data arriving on the northbound connection to the southbound service. */
            int err = -1;
            auto it = marshaller().marshal(stream);
            auto&[sfd, ssp] = stream;
            if(ssp->gcount() == 0)
                revents &= ~(POLLIN | POLLHUP);
            auto& buf = std::get<marshaller_type::south_format>(*it);
            const auto eof = ssp->eof();
            if(const auto p = buf.tellp(); eof || p > 0){
                for(auto conn = connections().begin(); conn < connections().end();){
                    if(auto s = conn->south.lock()){
                        if(s == ssp){
                            if(auto n = conn->north.lock()){
                                messages::msgtype t = {messages::DATA, 0};
                                if(eof) t.op = messages::STOP;
                                buf.seekg(0);
                                triggers().set(n->native_handle(), POLLOUT);
                                if(!_south_write(n, *conn, buf)){
                                    revents &= ~(POLLIN | POLLHUP);
                                    triggers().clear(sfd, POLLIN);
                                    return 0;
                                }
                                state_update(*conn, t, connection_type::clock_type::now());
                                if(conn->state == connection_type::CLOSED) return -1;
                                return 0;
                            } else conn = connections().erase(conn);
                        } else ++conn;
                    } else conn = connections().erase(conn);
                }
                if(eof) triggers().clear(sfd, POLLIN);
            } else if(p == 0) err = 0;
            return err;
        }
        void segment_connector::_south_state_handler(shared_south& interface, const south_type::stream_type& stream, event_mask& revents){
            const auto&[sfd, ssp] = stream;
            for(auto conn = connections().begin(); conn < connections().end();){
                if(auto s = conn->south.lock()){
                    if(s == ssp){
                        switch(conn->state){
                            case connection_type::CLOSED:
                                _south_err_handler(interface, stream, revents);
                                connections().erase(conn);
                                return;
                            case connection_type::HALF_CLOSED:
                                revents |= POLLHUP;
                                shutdown(sfd, SHUT_WR);
                            default: return;
                        }
                    } else ++conn;
                } else conn = connections().erase(conn);
            }
        }
        int segment_connector::_south_pollout_handler(south_type::stream_type& stream, event_mask& revents){
            if(revents & POLLERR) return -1;
            auto&[sfd, ssp] = stream;
            if(ssp->fail()) return -1;
            if(ssp->flush().bad()) return -1;
            if(ssp->tellp() == 0)
                triggers().clear(sfd, POLLOUT);
            revents &= ~(POLLOUT | POLLERR);
            return 0;
        }
        int segment_connector::_handle(shared_south& interface, south_type::stream_type& stream, event_mask& revents){
            int handled = 0;
            if(revents & (POLLOUT | POLLERR)){
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