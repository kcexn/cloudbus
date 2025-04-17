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
#include <unistd.h>
#include <fcntl.h>
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
            int fd = -1;
            while( (fd = accept(sockfd, addr, addrlen)) < 0 ){
                switch(errno){
                    case EINTR: continue;
                    default: return -errno;                    
                }
            }
            return set_flags(fd);
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
            std::array<char, 256> buf;
            while(auto gcount = is.readsome(buf.data(), buf.max_size()))
                if(os.write(buf.data(), gcount).bad())
                    return os;
            return os;
        }
        static std::ostream& stream_write(std::ostream& os, std::istream& is, std::streamsize maxlen){
            constexpr std::streamsize BUFLEN = 256;
            if(maxlen <= 0)
                return os;
            std::array<char, BUFLEN> buf;
            while( auto gcount = is.readsome(buf.data(), std::min(maxlen, BUFLEN)) ){
                if(os.write(buf.data(), gcount).bad())
                    return os;
                if( !(maxlen-=gcount) )
                    return os;
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
            const connector::events_type::iterator& ev,
            connector::events_type::iterator& put
        ){
            triggers.set(sockfd, POLLIN);
            auto it = std::find_if(events.begin(), events.end(),
                [&](const auto& e){
                    return e.fd == sockfd;
                }
            );
            if(it == events.end()){
                auto goff=ev-events.begin(), poff=put-events.begin();
                events.emplace_back(connector::event_type{sockfd, POLLIN, POLLIN});
                put = events.begin()+poff;
                return events.begin()+goff;
            }
            it->revents |= POLLIN;
            if(it < ev && it >= put)
                std::swap(*put++, *it);
            return ev;
        }

        connector::size_type connector::_handle(events_type& events){
            size_type handled = 0;
            auto put = events.begin();
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
                                    const auto&[sockfd, nsp] = stream;
                                    if(sockfd == ev->fd && ev->revents & (POLLOUT | POLLERR))
                                        for(auto& c: connections())
                                            if( !(c.north.owner_before(nsp) || nsp.owner_before(c.north)) )
                                                if(auto s = c.south.lock(); s && !s->eof())
                                                    ev = read_restart(s->native_handle(), triggers(), events, ev, put);
                                    return sockfd == ev->fd;
                                }
                            );
                            if(it != interface.streams().cend())
                                handled += _handle(static_cast<north_type&>(interface), *it, ev->revents);
                            return it != interface.streams().cend();
                        }
                    );
                    if(nit == north().end()){
                        std::find_if(
                                south().begin(),
                                south().end(),
                            [&](auto& interface){
                                auto it = std::find_if(
                                        interface.streams().cbegin(),
                                        interface.streams().cend(),
                                    [&](const auto& stream){
                                        const auto&[sockfd, ssp] = stream;
                                        if(sockfd==ev->fd && ev->revents & (POLLOUT | POLLERR))
                                            for(auto& c: connections())
                                                if( !(c.south.owner_before(ssp) || ssp.owner_before(c.south)) )
                                                    if(auto n = c.north.lock(); n && !n->eof())
                                                        ev = read_restart(n->native_handle(), triggers(), events, ev, put);
                                        return sockfd == ev->fd;
                                    }
                                );
                                if(it != interface.streams().cend())
                                    handled += _handle(static_cast<south_type&>(interface), *it, ev->revents);
                                return it != interface.streams().cend();
                            }
                        );
                    }
                    if(ev->revents)
                        *put++ = *ev;
                }
            }
            events.resize(put-events.begin());
            return handled + Base::_handle(events);
        }
        int connector::_route(marshaller_type::north_format& buf, const north_type& interface, const north_type::handle_type& stream, event_mask& revents){
            constexpr std::streamsize HDRLEN = sizeof(messages::msgheader);
            const auto&[nfd, nsp] = stream;
            const auto eof = nsp->eof();
            if(const auto *type = buf.type()){
                const auto *eid = buf.eid();
                const std::streamsize pos=buf.tellp(), gpos=buf.tellg();
                const std::streamsize seekpos =
                    (gpos <= HDRLEN)
                        ? HDRLEN
                        : gpos;
                const auto rem = buf.len()->length - pos;
                const auto time = connection_type::clock_type::now();
                auto conn = connections().begin(), cur=conn, end=connections().end();
                while((cur=conn++) != end){
                    if(cur->south.expired()) {
                        *cur = std::move(*(--end));
                        conn = cur;
                    } else if(cur->uuid == *eid && !(cur->north.owner_before(nsp) || nsp.owner_before(cur->north)) ) {
                        connections().resize(end-connections().begin());
                        if(auto s = cur->south.lock()){
                            if(auto sockfd = s->native_handle(); sockfd != s->BAD_SOCKET)
                                triggers().set(sockfd, POLLOUT);
                            if(!rem && (type->flags & messages::ABORT))
                            {
                                s->setstate(s->badbit);
                                for(int i=0; i<2; ++i)
                                    state_update(*cur, *type, time);
                                buf.setstate(buf.eofbit);
                                return eof ? -1 : 0;
                            }
                            if(cur->state != connection_type::CLOSED && pos > seekpos){
                                buf.seekg(seekpos);
                                if(!_north_write(s, buf))
                                    return clear_triggers(nfd, triggers(), revents, (POLLIN | POLLHUP));
                            }
                            if(!rem)
                                state_update(*cur, *type, time);
                        } else conn = connections().erase(cur);
                        if(!rem)
                            buf.setstate(buf.eofbit);
                        return eof ? -1 : 0;
                    }
                }
                connections().resize(end-connections().begin());
                if(!eof && !buf.eof() &&
                    (type->flags & messages::INIT) &&
                    seekpos==HDRLEN && pos > HDRLEN
                ){
                    buf.seekg(HDRLEN);
                    if(auto status = north_connect(interface, nsp, buf)){
                        if(status < 0)
                            return -1;
                    } else return clear_triggers(nfd, triggers(), revents, (POLLIN | POLLHUP));
                }
                if(!rem)
                    buf.setstate(buf.eofbit);
            }
            return eof ? -1 : 0;
        }
        int connector::_route(marshaller_type::south_format& buf, const south_type& interface, const south_type::handle_type& stream, event_mask& revents){
            auto&[sfd, ssp] = stream;
            const auto p = buf.tellp();
            if(const auto eof = ssp->eof(); eof || p > 0) {
                auto conn = connections().begin(), cur=conn, end=connections().end();
                while((cur=conn++) != end) {
                    if(cur->north.expired()) {
                        *cur = std::move(*(--end));
                        conn = cur;
                    } else if( !(cur->south.owner_before(ssp) || ssp.owner_before(cur->south)) ) {
                        if(auto n = cur->north.lock()){
                            connections().resize(end-connections().begin());
                            messages::msgtype t = {messages::DATA, 0};
                            if(eof) t.op = messages::STOP;
                            buf.seekg(0);
                            triggers().set(n->native_handle(), POLLOUT);
                            if(!_south_write(n, *cur, buf))
                                return clear_triggers(sfd, triggers(), revents, (POLLIN | POLLHUP));
                            state_update(*cur, t, connection_type::clock_type::now());
                            if(eof)
                                triggers().clear(sfd, POLLIN);
                            if(cur->state == connection_type::CLOSED)
                                return -1;
                            return 0;
                        } else {
                            *cur = std::move(*(--end));
                            conn=cur;
                        }
                    }
                }
                connections().resize(end-connections().begin());
                return -1;
            }
            if(p == 0) return 0;
            return -1;
        }
        std::streamsize connector::_north_connect(const north_type& interface, const north_type::stream_ptr& nsp, marshaller_type::north_format& buf){
            constexpr std::size_t SHRINK_THRESHOLD = 4096;
            auto& sbd = south().front();
            auto&[sfd, ssp] = sbd.make();
            sbd.register_connect(
                ssp,
                [&triggers=triggers()](
                    auto& hnd,
                    const auto *addr,
                    auto addrlen,
                    const std::string& protocol
                ){
                    auto&[sockfd, sptr] = hnd;
                    if(sockfd == sptr->BAD_SOCKET){
                        if(protocol == "TCP" || protocol == "UNIX") {
                            if( (sockfd=socket(addr->sa_family, SOCK_STREAM, 0)) < 0 )
                                throw std::system_error(
                                    std::error_code(errno, std::system_category()),
                                    "Unable to create a new socket."
                                );
                        } else throw std::invalid_argument("Unsupported transport protocol.");
                        sptr->native_handle() = set_flags(sockfd);
                    }
                    sptr->connectto(addr, addrlen);
                    triggers.set(sockfd, (POLLIN | POLLOUT));
                }
            );
            /* Address resolution only on the first pending connect. */
            if(sbd.addresses().empty() && sbd.npending()==1)
                resolver().resolve(sbd);
            const auto cur = connection_type::clock_type::now();
            connections().push_back(
                (buf.type()->op == messages::STOP)
                ? connection_type{{cur,cur,cur,{}}, *buf.eid(), nsp, ssp, connection_type::HALF_CLOSED}
                : connection_type{{cur,{},{},{}}, *buf.eid(), nsp, ssp, connection_type::HALF_OPEN}
            );
            if(connections().capacity() > SHRINK_THRESHOLD)
                connections().shrink_to_fit();
            return _north_write(ssp, buf);
        }
        void connector::_north_err_handler(north_type& interface, const north_type::handle_type& stream, event_mask& revents){
            const auto time = connection_type::clock_type::now();
            const auto&[nfd, nsp] = stream;
            auto conn=connections().begin(), cur=conn;
            while((cur=conn++) != connections().end()) {
                if(cur->south.expired()) {
                    conn = connections().erase(cur);
                } else if( !(cur->north.owner_before(nsp) || nsp.owner_before(cur->north)) ) {
                    if(auto s = cur->south.lock()) {
                        state_update(*cur, {messages::STOP, 0}, time);
                        triggers().set(s->native_handle(), POLLOUT);
                    } else conn = connections().erase(cur);
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
        int connector::_north_pollin_handler(const north_type& interface, const north_type::handle_type& stream, event_mask& revents){
            if(auto it = marshaller().unmarshal(stream); it != marshaller().north().end()){
                using stream_ptr = north_type::stream_ptr;
                using north_format = marshaller_type::north_format;
                if(!std::get<stream_ptr>(stream)->gcount())
                    revents &= ~(POLLIN | POLLHUP);
                return route(std::get<north_format>(*it), interface, stream, revents);
            } else return -1;
        }
        int connector::_north_accept_handler(north_type& interface, const north_type::handle_type& stream, event_mask& revents){
            using native_handle_type = north_type::native_handle_type;
            if(drain())
                return -1;
            int sockfd = -1, listenfd = std::get<native_handle_type>(stream);
            while( (sockfd = _accept(listenfd, nullptr, nullptr)) > -1 ){
                interface.make(sockfd, true);
                triggers().set(sockfd, POLLIN);
            }
            revents &= ~(POLLIN | POLLHUP);
            return (sockfd == -EWOULDBLOCK) ? 0 : -1;
        }
        int connector::_north_pollout_handler(const north_type::handle_type& stream, event_mask& revents){
            auto&[nfd, nsp] = stream;
            if(revents & (POLLERR | POLLNVAL))
                nsp->setstate(nsp->badbit);
            if(nsp->flush().bad())
                return -1;
            if(nsp->tellp() == 0)
                triggers().clear(nfd, POLLOUT);
            revents &= ~(POLLOUT | POLLERR | POLLNVAL);
            return 0;
        }
        connector::size_type connector::_handle(north_type& interface, const north_type::handle_type& stream, event_mask& revents){
            size_type handled = 0;
            if(revents & (POLLOUT | POLLERR | POLLNVAL)){
                ++handled;
                if(_north_pollout_handler(stream, revents)){
                    _north_err_handler(interface, stream, revents);
                }
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
        void connector::_south_err_handler(south_type& interface, const south_type::handle_type& stream, event_mask& revents){
            const auto&[sfd, ssp] = stream;
            const auto time = connection_type::clock_type::now();
            auto conn=connections().begin(), cur=conn;
            while((cur=conn++) != connections().end()){
                if(cur->north.expired()) {
                    conn = connections().erase(cur);
                } else if( !(cur->south.owner_before(ssp) || ssp.owner_before(cur->south)) ) {
                    if(auto n = cur->north.lock()) {
                        messages::msgheader stop{
                            cur->uuid, {1, sizeof(stop)},
                            {0,0}, {messages::STOP, 0}
                        };
                        if(cur->state < connection_type::CLOSED ||
                                (ssp->fail() || !ssp->eof())
                        ){
                            if(ssp->fail()){
                                stop.type.flags = messages::ABORT;
                                state_update(*cur, stop.type, time);
                            }
                            n->write(reinterpret_cast<const char*>(&stop), sizeof(stop));
                            triggers().set(n->native_handle(), POLLOUT);
                        }
                        state_update(*cur, stop.type, time);
                        if(cur->state == connection_type::CLOSED)
                            conn = connections().erase(cur);
                        break;
                    } else conn = connections().erase(cur);
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
        int connector::_south_pollin_handler(const south_type& interface, const south_type::handle_type& stream, event_mask& revents){
            if(auto it = marshaller().marshal(stream); it != marshaller().south().end()){
                using stream_ptr = south_type::stream_ptr;
                using south_format = marshaller_type::south_format;
                if(!std::get<stream_ptr>(stream)->gcount())
                    revents &= ~(POLLIN | POLLHUP);
                return route(std::get<south_format>(*it), interface, stream, revents);
            } else return -1;
        }
        void connector::_south_state_handler(south_type& interface, const south_type::handle_type& stream, event_mask& revents){
            const auto&[sfd, ssp] = stream;
            auto conn = connections().begin(), cur=conn;
            while((cur=conn++) != connections().end()) {
                if( !(cur->south.owner_before(ssp) || ssp.owner_before(cur->south)) ) {
                    switch(cur->state){
                        case connection_type::CLOSED:
                            conn = connections().erase(cur);
                            return _south_err_handler(interface, stream, revents);
                        case connection_type::HALF_CLOSED:
                            revents |= POLLHUP;
                            shutdown(sfd, SHUT_WR);
                        default: return;
                    }
                }
            }
        }
        int connector::_south_pollout_handler(const south_type::handle_type& stream, event_mask& revents){
            auto&[sfd, ssp] = stream;
            if(revents & (POLLERR | POLLNVAL))
                ssp->setstate(ssp->badbit);
            if(ssp->flush().bad())
                return -1;
            if(ssp->tellp() == 0)
                triggers().clear(sfd, POLLOUT);
            revents &= ~(POLLOUT | POLLERR | POLLNVAL);
            return 0;
        }
        connector::size_type connector::_handle(south_type& interface, const south_type::handle_type& stream, event_mask& revents){
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