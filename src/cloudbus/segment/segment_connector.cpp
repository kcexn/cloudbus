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
            std::array<char, 256> _buf;
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
            auto it = std::find_if(events.begin(), events.end(), [&](const auto& e){ return e.fd==sockfd; });
            if(it != events.end()){
                it->revents |= POLLIN;
                return ev;
            }
            auto off = ev-events.begin();
            events.emplace_back(connector::event_type{sockfd, POLLIN, POLLIN});
            return events.begin()+off;
        }
        
        connector::size_type connector::_handle(events_type& events){
            size_type handled = 0;
            for(auto ev = events.begin(); ev < events.end(); ++ev){
                if(ev->revents){
                    auto nit = std::find_if(
                            north().begin(),
                            north().end(),
                        [&](auto& interface){
                            auto it = std::find_if(
                                    interface.streams().cbegin(),
                                    interface.streams().cend(),
                                [&](const auto& stream){
                                    const auto&[sockfd, nsp] = *stream;
                                    if(sockfd == ev->fd && ev->revents & (POLLOUT | POLLERR))
                                        for(auto& c: connections())
                                            if(auto n = c.north.lock(); n && n==nsp)
                                                if(auto s = c.south.lock(); s && !s->eof())
                                                    ev = read_restart(s->native_handle(), triggers(), events, ev);
                                    return sockfd == ev->fd;
                                }
                            );
                            if(it != interface.streams().cend())
                                handled += _handle(static_cast<north_type&>(interface), *it, ev->revents);
                            return it != interface.streams().cend();
                        }
                    );
                    if(nit != north().end())
                        continue;
                    auto sit = std::find_if(
                            south().begin(),
                            south().end(),
                        [&](auto& interface){
                            auto it = std::find_if(
                                    interface.streams().cbegin(),
                                    interface.streams().cend(),
                                [&](const auto& stream){
                                    const auto&[sockfd, ssp] = *stream;
                                    if(sockfd==ev->fd && ev->revents & (POLLOUT | POLLERR))
                                        for(auto& c: connections())
                                            if(auto s = c.south.lock(); s && s==ssp)
                                                if(auto n = c.north.lock(); n && !n->eof())
                                                    ev = read_restart(n->native_handle(), triggers(), events, ev);
                                    return sockfd == ev->fd;
                                }
                            );
                            if(it != interface.streams().cend())
                                handled += _handle(static_cast<south_type&>(interface), *it, ev->revents);
                            return it != interface.streams().cend();
                        }
                    );
                    if(sit == south().end()){
                        triggers().clear(ev->fd);
                        ev->revents = 0;
                    }
                }
            }
            return handled;
        }
        int connector::_route(marshaller_type::north_format& buf, const north_type& interface, const north_type::handle_ptr& stream, event_mask& revents){
            constexpr std::streamsize HDRLEN = sizeof(messages::msgheader);
            const auto&[nfd, nsp] = *stream;
            const auto eof = nsp->eof();
            if(const auto *type = buf.type()){
                const auto *eid = buf.eid();
                const std::streamsize gpos=buf.tellg(), pos=buf.tellp();
                const std::streamsize seekpos = 
                    (gpos <= HDRLEN)
                        ? HDRLEN 
                        : gpos;
                const auto rem = buf.len()->length - pos;
                const auto time = connection_type::clock_type::now();
                for(auto conn = connections().begin(); conn < connections().end(); ++conn){
                    if(auto n = conn->north.lock(); n && conn->uuid==*eid && n==nsp){
                        if(auto s = conn->south.lock()){
                            triggers().set(s->native_handle(), POLLOUT);
                            if(!rem && (type->flags & messages::ABORT))
                            {
                                s->setstate(std::ios_base::badbit);
                                for(int i=0; i<2; ++i)
                                    state_update(*conn, *type, time);
                                buf.setstate(std::ios_base::eofbit);
                                return eof ? -1 : 0;
                            }
                            if(conn->state != connection_type::CLOSED && pos > seekpos){
                                buf.seekg(seekpos);
                                if(!_north_write(s, buf))
                                    return clear_triggers(nfd, triggers(), revents, (POLLIN | POLLHUP));
                            }
                        }
                        if(!rem){
                            state_update(*conn, *type, time);
                            buf.setstate(std::ios_base::eofbit);
                        }
                        if(conn->state == connection_type::CLOSED)
                            connections().erase(conn);
                        return eof ? -1 : 0;
                    }
                }
                if(!eof && !buf.eof() &&
                        (type->flags & messages::INIT) &&
                        seekpos==HDRLEN
                ){
                    buf.seekg(HDRLEN);
                    if(pos > HDRLEN && !north_connect(interface, nsp, buf))
                        return clear_triggers(nfd, triggers(), revents, (POLLIN | POLLHUP));
                }
                if(!rem)
                    buf.setstate(std::ios_base::eofbit);
            }
            return eof ? -1 : 0;
        }
        int connector::_route(marshaller_type::south_format& buf, const south_type& interface, const south_type::handle_ptr& stream, event_mask& revents){
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
                            if(conn->state == connection_type::CLOSED)
                                return -1;
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
        std::streamsize connector::_north_connect(const north_type& interface, const north_type::stream_ptr& nsp, marshaller_type::north_format& buf){
            constexpr std::size_t SHRINK_THRESHOLD = 4096;
            auto& sbd = south().front();
            auto& hnd = sbd.make();
            sbd.register_connect(hnd, [&triggers=triggers()](const auto& hnd, const auto *addr, auto addrlen, const std::string& protocol){
                auto&[sfd, ssp] = *hnd;
                if(!sfd){
                    auto& sockfd = ssp->native_handle();
                    if(protocol == "TCP" || protocol == "UNIX")
                        sockfd = socket(addr->sa_family, SOCK_STREAM, 0);
                    else throw std::invalid_argument("Unsupported transport protocol.");
                    sfd = set_flags(sockfd);
                    ssp->connectto(addr, addrlen);
                }
                triggers.set(sfd, (POLLIN | POLLOUT));
            });
            const auto n = connection_type::clock_type::now();
            auto& ssp = std::get<south_type::stream_ptr>(*hnd);
            connections().push_back(
                (buf.type()->op == messages::STOP)
                ? connection_type{*buf.eid(), nsp, ssp, connection_type::HALF_CLOSED, {n,n,n,{}}}
                : connection_type{*buf.eid(), nsp, ssp, connection_type::HALF_OPEN, {n,{},{},{}}}
            );
            if(connections().capacity() > SHRINK_THRESHOLD)
                connections().shrink_to_fit();
            return _north_write(ssp, buf);
        }
        void connector::_north_err_handler(north_type& interface, const north_type::handle_ptr& stream, event_mask& revents){
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
            interface.erase(stream);
        }
        std::streamsize connector::_north_write(const south_type::stream_ptr& s, marshaller_type::north_format& buf){
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
        int connector::_north_pollin_handler(const north_type& interface, const north_type::handle_ptr& stream, event_mask& revents){
            if(auto it = marshaller().unmarshal(stream); it != marshaller().north().end()){
                if(std::get<north_type::stream_ptr>(*stream)->gcount() == 0)
                    revents &= ~(POLLIN | POLLHUP);
                return route(std::get<marshaller_type::north_format>(*it), interface, stream, revents);
            } else return -1;
        }
        int connector::_north_accept_handler(north_type& interface, const north_type::handle_ptr& stream, event_mask& revents){
            if(drain())
                return -1;
            int sockfd = 0, listenfd = std::get<north_type::native_handle_type>(*stream);
            while((sockfd = _accept(listenfd, nullptr, nullptr)) >= 0){
                interface.make(sockfd, true);
                triggers().set(sockfd, POLLIN);
            }
            revents &= ~(POLLIN | POLLHUP);
            return 0;
        }
        int connector::_north_pollout_handler(const north_type::handle_ptr& stream, event_mask& revents){
            auto&[nfd, nsp] = *stream;
            if(revents & (POLLERR | POLLNVAL))
                nsp->setstate(std::ios_base::badbit);
            if(nsp->flush().bad())
                return -1;
            if(nsp->tellp() == 0)
                triggers().clear(nfd, POLLOUT);
            revents &= ~(POLLOUT | POLLERR | POLLNVAL);
            return 0;
        }
        connector::size_type connector::_handle(north_type& interface, const north_type::handle_ptr& stream, event_mask& revents){
            size_type handled = 0;
            if(revents & (POLLOUT | POLLERR | POLLNVAL)){
                ++handled;
                if(_north_pollout_handler(stream, revents))
                    _north_err_handler(interface, stream, revents);
            }
            if(revents & (POLLIN | POLLHUP)){
                ++handled;
                if(stream == interface.streams().front()) {
                    if(_north_accept_handler(interface, stream, revents))
                        _north_err_handler(interface, stream, revents);
                } else if(_north_pollin_handler(interface, stream, revents))
                    _north_err_handler(interface, stream, revents);
            }
            return handled;
        }
        void connector::_south_err_handler(south_type& interface, const south_type::handle_ptr& stream, event_mask& revents){
            const auto&[sfd, ssp] = *stream;
            const auto time = connection_type::clock_type::now();
            for(auto conn = connections().begin(); conn < connections().end(); ++conn){
                if(auto s = conn->south.lock(); s && s == ssp){
                    if(auto n = conn->north.lock()){
                        messages::msgheader stop{
                            conn->uuid, {1, sizeof(stop)},
                            {0,0}, {messages::STOP, 0}
                        };
                        if(conn->state < connection_type::CLOSED ||
                                (s->fail() || !s->eof())
                        ){
                            if(s->fail()){
                                stop.type.flags = messages::ABORT;
                                state_update(*conn, stop.type, time);
                            }
                            n->write(reinterpret_cast<const char*>(&stop), sizeof(stop));
                            triggers().set(n->native_handle(), POLLOUT);
                        }
                        state_update(*conn, stop.type, time);
                        if(conn->state == connection_type::CLOSED)
                            conn = --connections().erase(conn);
                        break;
                    } else conn = --connections().erase(conn);
                }
            }
            revents = 0;
            triggers().clear(sfd);
            interface.erase(stream);
        }
        std::streamsize connector::_south_write(const north_type::stream_ptr& n, const connection_type& conn, marshaller_type::south_format& buf){
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
        int connector::_south_pollin_handler(const south_type& interface, const south_type::handle_ptr& stream, event_mask& revents){
            auto it = marshaller().marshal(stream);
            auto& ssp = std::get<south_type::stream_ptr>(*stream);
            if(ssp->gcount() == 0)
                revents &= ~(POLLIN | POLLHUP);
            return route(std::get<marshaller_type::south_format>(*it), interface, stream, revents);
        }
        void connector::_south_state_handler(south_type& interface, const south_type::handle_ptr& stream, event_mask& revents){
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
        int connector::_south_pollout_handler(const south_type::handle_ptr& stream, event_mask& revents){
            auto&[sfd, ssp] = *stream;
            if(revents & (POLLERR | POLLNVAL))
                ssp->setstate(std::ios_base::badbit);
            if(ssp->flush().bad())
                return -1;
            if(ssp->tellp() == 0)
                triggers().clear(sfd, POLLOUT);
            revents &= ~(POLLOUT | POLLERR | POLLNVAL);
            return 0;
        }
        connector::size_type connector::_handle(south_type& interface, const south_type::handle_ptr& stream, event_mask& revents){
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