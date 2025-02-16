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
#include <sys/un.h>
#include <fcntl.h>
#ifdef DEBUG
    #include <chrono>
    #include <iostream>
#endif
namespace cloudbus {
    namespace controller {
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
        static void stream_write(std::ostream& os, std::istream& is){
            constexpr std::streamsize buflen = 256;
            std::array<char, buflen> buf;
            while(auto gcount = is.readsome(buf.data(), buflen))
                os.write(buf.data(), gcount);
        }              
        static void stream_write(std::ostream& os, std::istream& is, std::streamsize maxlen){
            constexpr std::streamsize buflen = 256;
            std::array<char, buflen> buf;
            while(auto gcount = is.readsome(buf.data(), std::min(maxlen, buflen))){
                os.write(buf.data(), gcount);
                maxlen -= gcount;
            }
        }

        control_connector::control_connector(trigger_type& triggers): Base(triggers){}
        control_connector::norths_type::iterator control_connector::make(norths_type& n, const north_type::address_type addr, north_type::size_type addrlen){
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
     
        control_connector::souths_type::iterator control_connector::make(souths_type& s, const south_type::address_type addr, south_type::size_type addrlen){
            s.push_back(make_south(addr, addrlen));
            return --s.end();
        }

        int control_connector::_handle(events_type& events){
            int handled = 0;
            for(auto& event: events){
                if(auto& revents = event.revents){
                    auto nit = std::find_if(north().begin(), north().end(), [&](auto& interface){
                        for(auto& stream: interface->streams()){
                            if(std::get<north_type::native_handle_type>(stream) == event.fd){
                                handled += _handle(interface, stream, revents);
                                return true;
                            }
                        }
                        return false;
                    });
                    if(nit != north().end()) continue;
                    auto sit = std::find_if(south().begin(), south().end(), [&](auto& interface){
                        for(auto& stream: interface->streams()){
                            if(std::get<south_type::native_handle_type>(stream) == event.fd){
                                handled += _handle(interface, stream, revents);
                                return true;
                            }
                        }
                        return false;
                    });
                    if(sit == south().end()){
                        triggers().clear(event.fd);
                        revents = 0;
                    }
                }
            }
            return handled;
        }

        void control_connector::_north_err_handler(shared_north& interface, north_type::stream_type& stream, event_mask& revents){
            revents = 0;
            triggers().clear(std::get<north_type::native_handle_type>(stream));
            interface->erase(stream);
        }
        void control_connector::_north_connect_handler(shared_north& interface, north_type::stream_ptr& nsp, marshaller_type::north_format& buf, event_mask& revents){
            auto posit = std::find(north().begin(), north().end(), interface);
            auto& sbd = south()[posit - north().begin()];
            south_type::stream_type s;
            south_type::stream_ptr ssp;
            south_type::native_handle_type fd;
            if(sbd->streams().empty()){
                s = sbd->make(sbd->address()->sa_family, SOCK_STREAM, 0);
                fd = set_flags(std::get<south_type::native_handle_type>(s));
                ssp = std::get<south_type::stream_ptr>(s);
                ssp->connectto(sbd->address(), sbd->addrlen());
            } else {
                s = sbd->streams().back();
                fd = std::get<south_type::native_handle_type>(s);
                ssp = std::get<south_type::stream_ptr>(s);
            }
            messages::msgheader head = {messages::make_uuid_v4(), {1, static_cast<std::uint16_t>(sizeof(head) + buf.tellp())}, {0,0}, {(nsp->eof()) ? messages::STOP : messages::DATA,0}};
            ssp->write(reinterpret_cast<char*>(&head), sizeof(head));
            buf.seekg(0);
            stream_write(*ssp, buf, buf.tellp());
            triggers().set(fd, (POLLIN | POLLOUT));
            connections().push_back(connection_type{head.eid, nsp, ssp, (head.type.op == messages::STOP) ? connection_type::HALF_CLOSED : connection_type::HALF_OPEN});        
        }     
        int control_connector::_north_pollin_handler(shared_north& interface, north_type::stream_type& stream, event_mask& revents){
            /* forward the data arriving on the northbound connection to the southbound service. */
            auto it = marshaller().unmarshal(stream);
            auto& nsp = std::get<north_type::stream_ptr>(stream);
            if(nsp->gcount() == 0)
                revents &= ~(POLLIN | POLLHUP);
            auto& buf = std::get<marshaller_type::north_format>(*it);
            if(nsp->eof() || buf.tellp() > 0){
                for(auto conn = connections().begin(); conn < connections().end();){
                    if(auto n = conn->north.lock()){
                        if(n == nsp){
                            if(auto s = conn->south.lock()){
                                messages::msgheader head = {conn->uuid, {1, static_cast<std::uint16_t>(sizeof(head) + buf.tellp())}, {0,0}, {(std::get<north_type::stream_ptr>(stream)->eof()) ? messages::STOP : messages::DATA,0}};
                                s->write(reinterpret_cast<char*>(&head), sizeof(head));
                                buf.seekg(0);
                                stream_write(*s, buf, buf.tellp());
                                triggers().set(s->native_handle(), POLLOUT);
                                if(head.type.op == messages::STOP){
                                    if(conn->state < connection_type::HALF_CLOSED) conn->state = connection_type::HALF_CLOSED;
                                    else conn->state = connection_type::CLOSED;
                                } else if (conn->state == connection_type::HALF_OPEN) conn->state = connection_type::OPEN;
                                ++conn;
                            } else conn = connections().erase(conn);
                        } else ++conn;
                    } else conn = connections().erase(conn);
                }
                if(!buf.eof() && buf.tellp() > buf.tellg())
                    _north_connect_handler(interface, nsp, buf, revents);
            }
            if(nsp->eof()) return -1;
            return 0;
        }
        int control_connector::_north_accept_handler(shared_north& interface, north_type::stream_type& stream, event_mask& revents){
            int sockfd = 0;
            auto listenfd = std::get<north_type::native_handle_type>(stream);
            while((sockfd = _accept(listenfd, nullptr, nullptr)) >= 0){
                interface->make(sockfd);
                triggers().set(sockfd, POLLIN);
            }
            revents &= ~(POLLIN | POLLHUP);
            return 0;
        }
        void control_connector::_north_state_handler(shared_north& interface, north_type::stream_type& stream, event_mask& revents){
            for(auto conn = connections().begin(); conn < connections().end();){
                if(auto n = conn->north.lock()){
                    if(n == std::get<north_type::stream_ptr>(stream)){
                        switch(conn->state){
                            case connection_type::CLOSED:
                                conn = connections().erase(conn);
                                _north_err_handler(interface, stream, revents);
                                break;
                            case connection_type::HALF_CLOSED:
                                revents |= POLLHUP;
                                shutdown(std::get<north_type::native_handle_type>(stream), SHUT_WR);
                            default:
                                ++conn;
                                break;
                        }
                    } else ++conn;
                } else conn = connections().erase(conn);
            }
        }
        int control_connector::_north_pollout_handler(shared_north& interface, north_type::stream_type& stream, event_mask& revents){
            if(revents & POLLERR) return -1;
            auto& nsp = std::get<north_type::stream_ptr>(stream);
            if(nsp->flush().bad()) return -1;
            if(nsp->tellp() == 0)
                triggers().clear(std::get<north_type::native_handle_type>(stream), POLLOUT);
            revents &= ~(POLLOUT | POLLERR);
            return 0;
        }
        int control_connector::_handle(shared_north& interface, north_type::stream_type& stream, event_mask& revents){
            int handled = 0;
            if(revents & (POLLOUT | POLLERR)){
                ++handled;
                if(_north_pollout_handler(interface, stream, revents)){
                    _north_err_handler(interface, stream, revents);
                    return handled;
                }
                _north_state_handler(interface, stream, revents);  
            }
            if(revents & (POLLIN | POLLHUP)){
                ++handled;
                if(stream == interface->streams().front()){
                    if(_north_accept_handler(interface, stream, revents)){
                        _north_err_handler(interface, stream, revents);
                        return handled;
                    }
                } else {
                    if(_north_pollin_handler(interface, stream, revents)){
                        _north_err_handler(interface, stream, revents);
                        return handled;
                    }
                }
            }     
            return handled;
        } 
        void control_connector::_south_err_handler(shared_south& interface, south_type::stream_type& stream, event_mask& revents){
            revents = 0;
            triggers().clear(std::get<south_type::native_handle_type>(stream));
            interface->erase(stream);
        }
        int control_connector::_south_pollin_handler(shared_south& interface, south_type::stream_type& stream, event_mask& revents){
            constexpr std::streamsize hdrlen = sizeof(messages::msgheader);         
            /* forward the data arriving on the northbound connection to the southbound service. */
            auto it = marshaller().marshal(stream);
            auto& ssp = std::get<south_type::stream_ptr>(stream);
            auto& buf = std::get<marshaller_type::south_format>(*it);
            if(ssp->gcount() == 0)
                revents &= ~(POLLIN | POLLHUP);
            if(buf.tellp() >= hdrlen){
                auto seekpos = buf.tellg();
                if(seekpos < hdrlen) seekpos = hdrlen;
                for(auto conn = connections().begin(); conn < connections().end();){
                    if(conn->uuid == *buf.eid()){
                        if(auto n = conn->north.lock()){
                            buf.seekg(seekpos);
                            stream_write(*n, buf);
                            triggers().set(n->native_handle(), POLLOUT);
                            if(buf.type()->op == messages::STOP){
                                if(conn->state < connection_type::HALF_CLOSED) conn->state = connection_type::HALF_CLOSED;
                                else conn->state = connection_type::CLOSED;
                            } else if(conn->state < connection_type::OPEN) conn->state = connection_type::OPEN;
                            ++conn;
                        } else conn = connections().erase(conn);
                    } else ++conn;
                }
                if(!buf.eof() && buf.tellp() != buf.tellg())
                    buf.setstate(std::ios_base::eofbit);
            }
            if(ssp->eof()) return -1;
            return 0;
        }
        void control_connector::_south_state_handler(shared_south& interface, south_type::stream_type& stream, event_mask& revents){
            std::size_t count = 0;
            for(auto conn = connections().begin(); conn < connections().end();){
                if(auto s = conn->south.lock()){
                    if(s == std::get<south_type::stream_ptr>(stream)){
                        if(conn->state != connection_type::CLOSED) {
                            ++count;
                            ++conn;
                        } else conn = connections().erase(conn);
                    } else ++conn;
                } else conn = connections().erase(conn);
            }
            if(count == 0) _south_err_handler(interface, stream, revents);
        }
        int control_connector::_south_pollout_handler(shared_south& interface, south_type::stream_type& stream, event_mask& revents){
            if(revents & POLLERR) return -1;
            auto& ssp = std::get<south_type::stream_ptr>(stream);
            if(ssp->flush().bad()) return -1;
            if(ssp->tellp() == 0)
                triggers().clear(std::get<south_type::native_handle_type>(stream), POLLOUT);
            revents &= ~(POLLOUT | POLLERR);
            return 0;
        }
        int control_connector::_handle(shared_south& interface, south_type::stream_type& stream, event_mask& revents){
            int handled = 0;          
            if(revents & (POLLOUT | POLLERR)){
                ++handled;
                if(_south_pollout_handler(interface, stream, revents)){
                    _south_err_handler(interface, stream, revents);
                    return handled;
                }
                _south_state_handler(interface, stream, revents);
            }
            if(revents & (POLLIN | POLLHUP)){
                ++handled;
                if(_south_pollin_handler(interface, stream, revents)){
                    _south_err_handler(interface, stream, revents);
                    return handled;
                }
            } 
            return handled;   
        }
    }
}