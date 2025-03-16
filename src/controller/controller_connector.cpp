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
#include "controller_connector.hpp"
#include <tuple>
#include <sys/un.h>
#include <fcntl.h>
#ifdef PROFILE
    #include <chrono>
    #include <iostream>
#endif
namespace cloudbus {
    namespace controller {
        static constexpr std::streamsize MAX_BUFSIZE = 65536 * 4096; /* 256MiB */
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
            connector::connection_type& conn,
            const messages::msgtype& type,
            const connector::connection_type::time_point time
        ){
            switch(conn.state){
                case connector::connection_type::HALF_OPEN:
                    conn.timestamps[++conn.state] = time;
                case connector::connection_type::OPEN:
                case connector::connection_type::HALF_CLOSED:
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
        static connector::connections_type::iterator write_prepare(
            connector::connections_type& connections, 
            connector::trigger_type& triggers, 
            const connector::north_type::stream_ptr& np, 
            const std::streamsize& tellp
        ){
            const std::streamsize pos = MAX_BUFSIZE-(tellp+sizeof(messages::msgheader));
            auto conn = connections.begin();
            while(conn < connections.end()){
                if(auto n = conn->north.lock(); n && n==np){
                    if(auto s = conn->south.lock()){
                        if(s->tellp() >= pos)
                            if(s->flush().bad())
                                return connections.end();
                        if(s->tellp() > pos)
                            return conn;
                        ++conn;
                    } else conn = connections.erase(conn);
                } else ++conn;
            }
            return conn;
        }
        static int clear_triggers(
            int sockfd,
            connector::trigger_type& triggers,
            connector::event_mask& revents,
            const connector::event_mask& mask
        ){
            revents &= ~mask;
            triggers.clear(sockfd, mask);
            return 0;
        }
        static connector::events_type::iterator read_restart(
            const int& sockfd,
            connector::trigger_type& triggers,
            connector::events_type& events,
            const connector::events_type::iterator& ev
        ){
            triggers.set(sockfd, POLLIN);
            auto it = std::find_if(events.begin(), events.end(), [&](auto& e){ return e.fd == sockfd; });
            if(it == events.end()){
                auto off = ev - events.begin();
                events.push_back(connector::event_type{sockfd, POLLIN, POLLIN});
                return events.begin() + off;
            } else it->revents |= POLLIN;
            return ev;
        }
        connector::size_type connector::_handle(events_type& events){
            size_type handled = 0;
            for(auto ev = events.begin(); ev < events.end(); ++ev){
                if(ev->revents){
                    auto nit = std::find_if(north().cbegin(), north().cend(), [&](const interface_type& interface){
                        for(auto& stream: interface->streams()){
                            auto&[sockfd, nsp] = *stream;
                            if(sockfd == ev->fd){
                                if(ev->revents & (POLLOUT | POLLERR))
                                    for(auto& c: connections())
                                        if(auto n = c.north.lock(); n && n==nsp)
                                            if(auto s = c.south.lock(); s && !s->eof())
                                                ev = read_restart(s->native_handle(), triggers(), events, ev);
                                handled += _handle(std::static_pointer_cast<north_type>(interface), stream, ev->revents);
                                return true;
                            }
                        }
                        return false;
                    });
                    if(nit != north().cend()) continue;
                    auto sit = std::find_if(south().cbegin(), south().cend(), [&](const interface_type& interface){
                        for(auto& stream: interface->streams()){
                            auto&[sockfd, ssp] = *stream;
                            if(sockfd == ev->fd){
                                if(ev->revents & (POLLOUT | POLLERR))
                                    for(auto& c: connections())
                                        if(auto s = c.south.lock(); s && s==ssp)
                                            if(auto n = c.north.lock(); n && !n->eof())
                                                ev = read_restart(n->native_handle(), triggers(), events, ev);
                                handled += _handle(std::static_pointer_cast<south_type>(interface), stream, ev->revents);
                                return true;
                            }
                        }
                        return false;
                    });
                    if(sit == south().cend()){
                        triggers().clear(ev->fd);
                        ev->revents = 0;
                    }
                }
            }
            return handled;
        }
        int connector::_route(marshaller_type::north_format& buf, const shared_north& interface, const north_type::handle_ptr& stream, event_mask& revents){
            auto&[nfd, nsp] = *stream;
            const auto eof = nsp->eof();
            if(const auto p = buf.tellp(); eof || p > 0){
                messages::msgheader head = {
                    {},{1, static_cast<std::uint16_t>(static_cast<std::uint16_t>(p)+sizeof(head))},
                    {0,0},{(eof) ? messages::STOP : messages::DATA, 0}
                };
                if(write_prepare(connections(), triggers(), nsp, p) != connections().end())
                    return clear_triggers(nfd, triggers(), revents, (POLLIN | POLLHUP));
                const auto time = connector::connection_type::clock_type::now();
                std::size_t connected = 0;
                for(auto& c: connections()){
                    if(auto n = c.north.lock(); n && n==nsp && c.state < connector::connection_type::CLOSED){
                        if(auto s = c.south.lock()){
                            if(buf.tellg() == 0) 
                                state_update(c, head.type, time);
                            ++connected;
                            head.eid = c.uuid;
                            s->write(reinterpret_cast<const char*>(&head), sizeof(head));
                            stream_write(*s, buf.seekg(0), p);
                            triggers().set(s->native_handle(), POLLOUT);
                        }
                    }
                }
                if(!connected){
                    if(eof) return -1;
                    else if(!north_connect(interface, nsp, buf))
                        return clear_triggers(nfd, triggers(), revents, (POLLIN | POLLHUP));
                }
            }
            if(eof) triggers().clear(nfd, POLLIN);
            return 0;
        }
        int connector::_route(marshaller_type::south_format& buf, const shared_south& interface, const south_type::handle_ptr& stream, event_mask& revents){
            constexpr std::streamsize HDRLEN = sizeof(messages::msgheader);
            auto&[sfd, ssp] = *stream;
            const auto *eid = buf.eid();
            if(const auto *type = eid != nullptr ? buf.type() : nullptr; type != nullptr){
                const std::streamsize seekpos = 
                    (buf.tellg() <= HDRLEN)
                    ? HDRLEN
                    : static_cast<std::streamsize>(buf.tellg());
                const std::streamsize pos = buf.tellp();
                const auto time = connection_type::clock_type::now();
                for(auto conn = connections().begin(); conn < connections().end();){
                    if(auto s = conn->south.lock(); !messages::uuid_cmpnode(&conn->uuid, eid) && s && s == ssp){
                        if(conn->state == connection_type::CLOSED){
                            if(type->op == messages::STOP)
                                conn = connections().erase(conn);
                            break;
                        } else if(auto n = conn->north.lock()){
                            if(conn->state==connection_type::HALF_CLOSED && !n->eof())
                                break;
                            buf.seekg(seekpos);
                            triggers().set(n->native_handle(), POLLOUT);
                            if(pos > seekpos && !_south_write(n, buf))
                                return clear_triggers(sfd, triggers(), revents, (POLLIN | POLLHUP));
                            if(!buf.eof() && buf.tellg()==buf.len()->length)
                                buf.setstate(std::ios_base::eofbit);
                            if(seekpos == HDRLEN){
                                state_update(*conn, *type, time);
                                messages::msgheader stop = {
                                    *eid, {1, sizeof(stop)},
                                    {0,0},{messages::STOP, 0}
                                };
                                for(auto c=connections().begin(); c < connections().end() && c->state < connection_type::HALF_CLOSED && Base::mode() == Base::HALF_DUPLEX; ++c){
                                    // This is a very awkward way to do this, but I have implemented it like this to keep 
                                    // open the option of implementing UDP transport. With unreliable transports, it is 
                                    // necessary to retry control messages until after the remote end sends back an ACK.
                                    if(auto sp = c->south.lock(); c->uuid==*eid && sp && sp != ssp){
                                        sp->write(reinterpret_cast<const char*>(&stop), sizeof(stop));
                                        triggers().set(sp->native_handle(), POLLOUT);
                                        state_update(*c, stop.type, time);
                                        state_update(*c, stop.type, time);
                                    }
                                }
                            }
                            break;
                        } else conn = connections().erase(conn);
                    } else ++conn;
                }
                if(!buf.eof() && pos==buf.len()->length)
                    buf.setstate(std::ios_base::eofbit);
            }
            if(ssp->eof()) return -1;
            return 0;
        }
        std::streamsize connector::_north_connect(const shared_north& interface, const north_type::stream_ptr& nsp, marshaller_type::north_format& buf){
            constexpr std::size_t SHRINK_THRESHOLD = 1024;
            const auto n = connection_type::clock_type::now();
            auto eid = messages::make_uuid_v7();
            connections_type connect;
            for(auto& sbd: south()){
                auto& hnd = sbd->streams().empty() ? sbd->make() : sbd->streams().back();
                sbd->register_connect(hnd, [&triggers=triggers()](const auto& hnd, const auto *addr, auto addrlen, const std::string& protocol){
                    auto&[sfd, ssp] = *hnd;
                    if(!sfd){
                        auto& sock = ssp->native_handle();
                        if(protocol == "TCP" || protocol == "UNIX")
                            sock = socket(addr->sa_family, SOCK_STREAM, 0);
                        else throw std::invalid_argument("Unsupported transport protocol.");
                        sfd = set_flags(sock);
                        ssp->connectto(addr, addrlen);
                    }
                    triggers.set(sfd, (POLLIN | POLLOUT));
                });
                if(Base::mode() == Base::FULL_DUPLEX){
                    if((eid.clock_seq_reserved & messages::CLOCK_SEQ_MAX) == messages::CLOCK_SEQ_MAX)
                        eid.clock_seq_reserved &= ~messages::CLOCK_SEQ_MAX;
                    else ++eid.clock_seq_reserved;
                }
                auto& ssp = std::get<south_type::stream_ptr>(*hnd);
                connect.push_back(
                    (nsp->eof())
                    ? connection_type{eid, nsp, ssp, connection_type::HALF_CLOSED, {n,n,n,{}}}
                    : connection_type{eid, nsp, ssp, connection_type::HALF_OPEN, {n,{},{},{}}}
                );
            }
            connections().insert(connections().cend(), connect.cbegin(), connect.cend());
            if(connections().capacity() > SHRINK_THRESHOLD 
                && connections().size() < connections().capacity()/2)
                connections().shrink_to_fit();
            const std::streamsize pos = buf.tellp();
            messages::msgheader head = {
                {},{1, static_cast<std::uint16_t>(pos+sizeof(head))},
                {0,0},{(nsp->eof()) ? messages::STOP : messages::DATA, 0}
            };
            if(write_prepare(connect, triggers(), nsp, pos) == connect.end()){
                for(auto& c: connect){
                    if(auto s = c.south.lock()){
                        head.eid = c.uuid;
                        s->write(reinterpret_cast<const char*>(&head), sizeof(head));
                        stream_write(*s, buf.seekg(0), pos);
                    }
                }
                return sizeof(head) + pos;
            }
            return 0;
        }
        void connector::_north_err_handler(const shared_north& interface, const north_type::handle_ptr& stream, event_mask& revents){
            messages::msgheader head = {
                {}, {1, static_cast<std::uint16_t>(sizeof(head))},
                {0,0},{messages::STOP, 0}
            };
            const auto time = connection_type::clock_type::now();
            const auto&[nfd, nsp] = *stream;
            for(auto conn = connections().begin(); conn < connections().end(); ++conn){
                if(auto n = conn->north.lock(); n && n == nsp){
                    if(auto s = conn->south.lock()){
                        triggers().set(s->native_handle(), POLLOUT);
                        if(conn->state < connection_type::HALF_CLOSED || (!n->eof() && conn->state == connection_type::HALF_CLOSED)){
                            head.eid = conn->uuid;
                            s->write(reinterpret_cast<const char*>(&head), sizeof(head));
                        }
                        state_update(*conn, head.type, time);
                        if(conn->state == connection_type::CLOSED)
                            conn = --connections().erase(conn);
                    } else conn = --connections().erase(conn);
                }
            }            
            revents = 0;
            triggers().clear(nfd);
            interface->erase(stream);
        }
        int connector::_north_pollin_handler(const shared_north& interface, const north_type::handle_ptr& stream, event_mask& revents){
            auto it = marshaller().unmarshal(stream);
            if(std::get<north_type::stream_ptr>(*stream)->gcount() == 0)
                revents &= ~(POLLIN | POLLHUP);
            return route(std::get<marshaller_type::north_format>(*it), interface, stream, revents);
        }
        int connector::_north_accept_handler(const shared_north& interface, const north_type::handle_ptr& stream, event_mask& revents){
            if(drain())
                return -1;
            int sockfd = 0, listenfd = std::get<north_type::native_handle_type>(*stream);
            while((sockfd = _accept(listenfd, nullptr, nullptr)) >= 0){
                interface->make(sockfd, true);
                triggers().set(sockfd, POLLIN);
            }
            revents &= ~(POLLIN | POLLHUP);
            return 0;
        }
        void connector::_north_state_handler(const shared_north& interface, const north_type::handle_ptr& stream, event_mask& revents){
            const auto&[nfd, nsp] = *stream;
            int closed = 0;
            connections_type states;
            for(auto& c: connections()){
                if(auto n = c.north.lock(); n && n==nsp){
                    auto it = std::find_if(states.begin(), states.end(), [&](auto& sc){ return !messages::uuid_cmpnode(&sc.uuid, &c.uuid); });
                    if(it == states.end())
                        states.push_back(c);
                    else if(c.state < it->state)
                        it->state = c.state;
                }
            }
            const auto time = connection_type::clock_type::now();
            messages::msgheader stop = {
                {},{1, sizeof(stop)},
                {0,0},{messages::STOP, 0}
            };
            for(auto conn=connections().begin(); conn < connections().end(); ++conn){
                if(auto n = conn->north.lock(); n && n==nsp){
                    auto it = std::find_if(states.cbegin(), states.cend(), [&](const auto& sc){ return !messages::uuid_cmpnode(&sc.uuid, &conn->uuid); });
                    if(it == states.cend()) continue;
                    switch(auto s = conn->south.lock(); it->state){
                        case connection_type::HALF_OPEN:
                        case connection_type::OPEN:
                            if(s && conn->state == connection_type::HALF_CLOSED){
                                stop.eid = conn->uuid;
                                s->write(reinterpret_cast<const char*>(&stop), sizeof(stop));
                                triggers().set(s->native_handle(), POLLOUT);
                                state_update(*conn, stop.type, time);
                            }
                            break;
                        case connection_type::HALF_CLOSED:
                            revents |= POLLHUP;
                            shutdown(nfd, SHUT_WR);
                            return;
                        case connection_type::CLOSED:
                            if(s) triggers().set(s->native_handle(), POLLOUT);
                            conn = --connections().erase(conn);
                            closed = -1;
                        default: break;
                    }
                }
            }
            if(closed) return _north_err_handler(interface, stream, revents);
        }
        int connector::_north_pollout_handler(const north_type::handle_ptr& stream, event_mask& revents){
            const auto&[nfd, nsp]=*stream;
            nsp->flush();
            if(nsp->fail() || revents & (POLLERR | POLLNVAL))
                return -1;
            if(nsp->tellp() == 0)
                triggers().clear(nfd, POLLOUT);
            revents &= ~(POLLOUT | POLLERR | POLLNVAL);
            return 0;
        }
        connector::size_type connector::_handle(const shared_north& interface, const north_type::handle_ptr& stream, event_mask& revents){
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
        void connector::_south_err_handler(const shared_south& interface, const south_type::handle_ptr& stream, event_mask& revents){
            const auto&[sfd, ssp] = *stream;
            for(auto conn = connections().begin(); conn < connections().end(); ++conn){
                if(auto s = conn->south.lock(); s && s == ssp){
                    if(auto n = conn->north.lock()){
                        state_update(*conn, {messages::STOP, 0}, connection_type::clock_type::now());
                        triggers().set(n->native_handle(), POLLOUT);
                    } else conn = --connections().erase(conn);
                }
            }
            revents = 0;
            triggers().clear(sfd);
            interface->erase(stream);
        }
        std::streamsize connector::_south_write(const north_type::stream_ptr& n, marshaller_type::south_format& buf){
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
        int connector::_south_pollin_handler(const shared_south& interface, const south_type::handle_ptr& stream, event_mask& revents){
            auto it = marshaller().marshal(stream);
            if(std::get<south_type::stream_ptr>(*stream)->gcount() == 0)
                revents &= ~(POLLIN | POLLHUP);
            return route(std::get<marshaller_type::south_format>(*it), interface, stream, revents);
        }
        int connector::_south_state_handler(const south_type::handle_ptr& stream){
            std::size_t count = 0;
            const auto& ssp = std::get<south_type::stream_ptr>(*stream);
            for(auto conn = connections().begin(); conn < connections().end(); ++conn){
                if(auto s = conn->south.lock(); s && s==ssp){
                    if(conn->state != connection_type::CLOSED)
                        ++count;
                    else conn = --connections().erase(conn);
                }
            }
            if(count == 0) return -1;
            return 0;
        }
        int connector::_south_pollout_handler(const south_type::handle_ptr& stream, event_mask& revents){
            const auto&[sfd, ssp] = *stream;
            ssp->flush();
            if(ssp->fail() || revents & (POLLERR | POLLNVAL))
                return -1;
            if(ssp->tellp() == 0)
                triggers().clear(sfd, POLLOUT);
            revents &= ~(POLLOUT | POLLERR | POLLNVAL);
            return 0;
        }
        connector::size_type connector::_handle(const shared_south& interface, const south_type::handle_ptr& stream, event_mask& revents){
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