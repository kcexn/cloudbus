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
#include <netinet/tcp.h>
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
            int fd = 0;
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
            constexpr std::streamsize BUFSIZE = 256;
            if(maxlen <= 0)
                return os;
            std::array<char, BUFSIZE> buf;
            while( auto gcount = is.readsome(buf.data(), std::min(maxlen, BUFSIZE)) ){
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
                    auto sit = std::find_if(
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
                    if(sit == south().end()){
                        std::find_if(
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
                    }
                    if(ev->revents)
                        *put++ = *ev;
                }
            }
            events.resize(put-events.begin());
            return handled + Base::_handle(events);
        }
        static connector::connections_type::const_iterator write_prepare(
            connector::connections_type& connections,
            const connector::north_type::stream_ptr& np,
            const std::streamsize& tellp
        ){
            const std::streamsize pos = MAX_BUFSIZE-(tellp+sizeof(messages::msgheader));
            auto conn=connections.begin(), cur=conn, end=connections.end();
            while((cur=conn) != end) {
                ++conn;
                if(cur->south.expired()) {
                    *cur = std::move(*(--end));
                    conn = cur;
                } else if( !(cur->north.owner_before(np) || np.owner_before(cur->north)) ) {
                    if(auto s = cur->south.lock()) {
                        if(s->fail())
                            continue;
                        if(s->tellp() >= pos && s->flush().bad())
                            continue;
                        if(s->tellp() > pos)
                            return cur;
                    } else {
                        *cur = std::move(*(--end));
                        conn = cur;
                    }
                }
            }
            connections.resize(end-connections.begin());
            return connections.cend();
        }
        int connector::_route(marshaller_type::north_format& buf, const north_type& interface, const north_type::handle_type& stream, event_mask& revents){
            constexpr std::streamsize HDRLEN = sizeof(messages::msgheader);
            auto&[nfd, nsp] = stream;
            const auto eof = nsp->eof();
            if(const auto p = buf.tellp(); eof || p > 0){
                messages::msgheader head = {
                    {}, {1, static_cast<std::uint16_t>(p+HDRLEN)},
                    {0,0}, {eof ? messages::STOP : messages::DATA, 0}
                };
                if(write_prepare(connections(), nsp, p) != connections().cend())
                    return clear_triggers(nfd, triggers(), revents, (POLLIN | POLLHUP));
                const auto time = connection_type::clock_type::now();
                std::size_t connected = 0;
                auto conn=connections().begin(), cur=conn, end=connections().end();
                while((cur=conn) != end) {
                    ++conn;
                    if( !(cur->north.owner_before(nsp) || nsp.owner_before(cur->north)) ) {
                        if(auto s = cur->south.lock()) {
                            if(++connected && cur->state != connection_type::CLOSED){
                                head.eid = cur->uuid;
                                s->write(reinterpret_cast<const char*>(&head), sizeof(head));
                                stream_write(*s, buf.seekg(0), p);
                                if(auto sockfd = s->native_handle(); sockfd != s->BAD_SOCKET)
                                    triggers().set(sockfd, POLLOUT);
                                state_update(*cur, head.type, time);
                            }
                        }
                    }
                }
                connections().resize(end-connections().begin());
                if(!eof && !connected){
                    if(auto status = north_connect(interface, nsp, buf)){
                        if(status < 0)
                            return -1;
                    } else return clear_triggers(nfd, triggers(), revents, (POLLIN | POLLHUP));
                }
            }
            if(eof){
                /* run state handler in-case session is closed. */
                triggers().set(nfd, POLLOUT);
                triggers().clear(nfd, POLLIN);
            }
            return 0;
        }
        int connector::_route(marshaller_type::south_format& buf, const south_type& interface, const south_type::handle_type& stream, event_mask& revents){
            constexpr std::streamsize HDRLEN = sizeof(messages::msgheader);
            const auto&[sfd, ssp] = stream;
            const auto eof = ssp->eof();
            if(const auto *type = buf.type()){
                const auto *eid = buf.eid();
                const std::streamsize pos=buf.tellp(), gpos=buf.tellg();
                const std::streamsize seekpos =
                    (gpos <= HDRLEN)
                    ? HDRLEN
                    : gpos;
                const auto rem = buf.len()->length - pos;
                const auto time = connection_type::clock_type::now();
                messages::msgheader stop = {
                    *eid, {1, sizeof(stop)},
                    {0,0},{messages::STOP, messages::ABORT}
                };
                auto conn=connections().begin(), cur=conn, end=connections().end();
                while((cur=conn) != end) {
                    ++conn;
                    if(cur->north.expired()){
                        *cur = std::move(*(--end));
                        conn = cur;
                    } else if (
                        !messages::uuidcmp_node(&cur->uuid, eid) &&
                        !(cur->south.owner_before(ssp) || ssp.owner_before(cur->south))
                    ){
                        if(auto n = cur->north.lock()) {
                            if(rem && !eof)
                                break;
                            if(cur->state == connection_type::CLOSED)
                                break;
                            if(cur->state == connection_type::HALF_CLOSED &&
                                !n->eof() && !(type->flags & messages::ABORT)
                            ){ 
                                break;
                            }
                            connections().resize(end-connections().begin());
                            triggers().set(n->native_handle(), POLLOUT);
                            if(pos > seekpos){
                                buf.seekg(seekpos);
                                if(!_south_write(n, buf))
                                    return clear_triggers(sfd, triggers(), revents, (POLLIN | POLLHUP));
                            }
                            auto prev = cur->state;
                            state_update(*cur, *type, time);
                            if(type->flags & messages::ABORT)
                                state_update(*cur, *type, time);
                            if(mode() == HALF_DUPLEX &&
                                    prev == connection_type::HALF_OPEN &&
                                    cur->state == connection_type::OPEN &&
                                    seekpos == HDRLEN && pos > seekpos
                            ){
                                for(auto& c: connections()){
                                    if(c.uuid == *eid && c.state < connection_type::HALF_CLOSED){
                                        if(c.south.owner_before(ssp) || ssp.owner_before(c.south)){
                                            if(auto sp = c.south.lock()) {
                                                sp->write(reinterpret_cast<const char*>(&stop), sizeof(stop));
                                                triggers().set(sp->native_handle(), POLLOUT);
                                                for(int i=0; i<2; ++i)
                                                    state_update(c, stop.type, time);
                                            }
                                        }
                                    }
                                }
                            }
                            buf.setstate(buf.eofbit);
                            return eof ? -1 : 0;
                        } else {
                            *cur = std::move(*(--end));
                            conn = cur;
                        }
                    }
                }
                connections().resize(end-connections().begin());
                if(!rem) {
                    buf.setstate(buf.eofbit);
                    if(cur==connections().end()) {
                        ssp->write(reinterpret_cast<char*>(&stop), sizeof(stop));
                        triggers().set(ssp->native_handle(), POLLOUT);
                    }
                }
            }
            return (eof) ? -1 : 0;
        }
        std::streamsize connector::_north_connect(const north_type& interface, const north_type::stream_ptr& nsp, marshaller_type::north_format& buf){
            constexpr std::size_t SHRINK_THRESHOLD = 4096;
            const auto n = connection_type::clock_type::now();
            auto eid = messages::make_uuid_v7();
            if(eid == messages::uuid{})
                return -1;
            connections_type connect;
            for(auto& sbd: south()){
                bool empty = sbd.streams().empty();
                auto&[sockfd, sptr] = empty ? sbd.make() : sbd.streams().back();
                if(empty){
                    sbd.register_connect(
                        sptr,
                        [&](
                            auto& hnd,
                            const auto *addr,
                            auto addrlen,
                            const std::string& protocol
                        ){
                            auto&[sfd, ssp] = hnd;
                            if(sfd == ssp->BAD_SOCKET){
                                if(protocol == "TCP" || protocol == "UNIX") {
                                    if( (sfd=socket(addr->sa_family, SOCK_STREAM, 0)) < 0 )
                                        throw std::system_error(
                                            std::error_code(errno, std::system_category()), 
                                            "Unable to create a new socket."
                                        );
                                    if(protocol == "TCP"){
                                        int nodelay = 1;
                                        if(setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)))
                                            throw std::system_error(
                                                std::error_code(errno, std::system_category()),
                                                "Unable to set TCP_NODELAY on socket."
                                            );
                                    }
                                } else throw std::invalid_argument("Unsupported transport protocol.");
                                ssp->native_handle() = set_flags(sfd);
                            }
                            ssp->connectto(addr, addrlen);
                            triggers().set(sfd, (POLLIN | POLLOUT));
                        }
                    );
                    /* Address resolution only on the first pending connect. */
                    if(sbd.addresses().empty() && sbd.npending()==1)
                        resolver().resolve(sbd);
                }
                if(mode() == FULL_DUPLEX){
                    if((eid.clock_seq_reserved & messages::CLOCK_SEQ_MAX) == messages::CLOCK_SEQ_MAX)
                        eid.clock_seq_reserved &= ~messages::CLOCK_SEQ_MAX;
                    else ++eid.clock_seq_reserved;
                }
                connect.push_back(
                    (nsp->eof())
                    ? connection_type{{n,n,n,{}}, eid, nsp, sptr, connection_type::HALF_CLOSED}
                    : connection_type{{n,{},{},{}}, eid, nsp, sptr, connection_type::HALF_OPEN}
                );
            }
            connections().insert(connections().cend(), connect.cbegin(), connect.cend());
            if(connections().capacity() > SHRINK_THRESHOLD)
                connections().shrink_to_fit();
            const std::streamsize pos = buf.tellp();
            messages::msgheader head;
            head.len = {1, static_cast<std::uint16_t>(pos + sizeof(head))};
            head.version = {0,0};
            head.type = (nsp->eof()) ?
                messages::msgtype{messages::STOP, messages::INIT} :
                messages::msgtype{messages::DATA, messages::INIT};
            if(write_prepare(connect, nsp, pos) == connect.cend()){
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
        void connector::_north_err_handler(north_type& interface, const north_type::handle_type& stream, event_mask& revents){
            messages::msgheader stop = {
                {}, {1, static_cast<std::uint16_t>(sizeof(stop))},
                {0,0},{messages::STOP, 0}
            };
            const auto time = connection_type::clock_type::now();
            const auto&[nfd, nsp] = stream;
            auto conn = connections().begin(), cur=conn, end=connections().end();
            while((cur=conn) != end) {
                ++conn;
                if( !(cur->north.owner_before(nsp) || nsp.owner_before(cur->north)) ) {
                    if(auto s = cur->south.lock()) {
                        triggers().set(s->native_handle(), POLLOUT);
                        if(
                            cur->state < connection_type::CLOSED &&
                            (nsp->fail() || !nsp->eof())
                        ){
                            stop.eid = cur->uuid;
                            if(nsp->fail()){
                                stop.type.flags = messages::ABORT;
                                state_update(*cur, stop.type, time);
                            }
                            s->write(reinterpret_cast<const char*>(&stop), sizeof(stop));
                        }
                        state_update(*cur, stop.type, time);
                        if(cur->state == connection_type::CLOSED) {
                            *cur = std::move(*(--end));
                            conn = cur;
                        }
                    }
                }
            }
            connections().resize(end-connections().begin());
            revents = 0;
            triggers().clear(nfd);
            interface.erase(stream);
        }
        int connector::_north_pollin_handler(const north_type& interface, const north_type::handle_type& stream, event_mask& revents){
            auto it = marshaller().unmarshal(stream);
            if(std::get<north_type::stream_ptr>(stream)->gcount() == 0)
                revents &= ~(POLLIN | POLLHUP);
            return route(std::get<marshaller_type::north_format>(*it), interface, stream, revents);
        }
        int connector::_north_accept_handler(north_type& interface, const north_type::handle_type& stream, event_mask& revents){
            using native_handle_type = north_type::native_handle_type;
            if(drain())
                return -1;
            int sockfd = -1;
            auto listenfd = std::get<native_handle_type>(stream);
            while( (sockfd = _accept(listenfd, nullptr, nullptr)) > -1) {
                interface.make(sockfd, true);
                if(interface.protocol() == "TCP") {
                    int nodelay = 1;
                    if(setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(int)))
                        throw std::system_error(
                            std::error_code(errno, std::system_category()),
                            "Unable to set TCP_NODELAY socket option."
                        );
                }
                triggers().set(sockfd, POLLIN);
            }
            revents &= ~(POLLIN | POLLHUP);
            return (sockfd == -EWOULDBLOCK) ? 0 : -1;
        }
        static int session_state(const connector::connections_type& connections, const connector::north_type::stream_ptr& nsp){
            using connection = connector::connection_type;
            int state = connection::CLOSED;
            for(const auto& c: connections){
                if( !(c.north.owner_before(nsp) || nsp.owner_before(c.north)) ) {
                    if(c.state == connection::OPEN) {
                        state = c.state;
                    } else if (state != connection::OPEN) {
                        state = std::min(state, c.state);
                    }
                }
            }
            return state;
        }
        void connector::_north_state_handler(north_type& interface, const north_type::handle_type& stream, event_mask& revents){
            const auto&[nfd, nsp] = stream;
            switch(session_state(connections(), nsp)){
                case connection_type::CLOSED:
                {
                    auto conn = connections().begin(), cur=conn, end=connections().end();
                    while((cur=conn) != end){
                        ++conn;
                        if( !(cur->north.owner_before(nsp) || nsp.owner_before(cur->north)) ) {
                            if(auto s = cur->south.lock())
                                triggers().set(s->native_handle(), POLLOUT);
                            *cur = std::move(*(--end));
                            conn = cur;
                        }
                    }
                    connections().resize(end-connections().begin());
                    return _north_err_handler(interface, stream, revents);
                }
                case connection_type::HALF_CLOSED:
                    revents |= POLLHUP;
                    shutdown(nfd, SHUT_WR);
                default: break;
            }
        }
        int connector::_north_pollout_handler(const north_type::handle_type& stream, event_mask& revents){
            const auto&[nfd, nsp]=stream;
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
                if(_north_pollout_handler(stream, revents))
                    _north_err_handler(interface, stream, revents);
                else _north_state_handler(interface, stream, revents);
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
            auto conn = connections().begin(), cur=conn, end=connections().end();
            while((cur=conn) != end){
                ++conn;
                if( !(cur->south.owner_before(ssp) || ssp.owner_before(cur->south)) ){
                    if(auto n = cur->north.lock()) {
                        state_update(*cur, {messages::STOP, 0}, time);
                        triggers().set(n->native_handle(), POLLOUT);
                    }
                }
            }
            revents = 0;
            triggers().clear(sfd);
            interface.erase(stream);
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
        int connector::_south_pollin_handler(const south_type& interface, const south_type::handle_type& stream, event_mask& revents){
            if(auto it = marshaller().marshal(stream); it != marshaller().south().end()){
                if(std::get<south_type::stream_ptr>(stream)->gcount() == 0)
                    revents &= ~(POLLIN | POLLHUP);
                return route(std::get<marshaller_type::south_format>(*it), interface, stream, revents);
            } else return -1;
        }
        int connector::_south_state_handler(const south_type::handle_type& stream){
            std::size_t count = 0;
            const auto& ssp = std::get<south_type::stream_ptr>(stream);
            auto conn = connections().begin(), cur=conn, end=connections().end();
            while((cur=conn) != end){
                ++conn;
                if( !(cur->south.owner_before(ssp) || ssp.owner_before(cur->south)) ) {
                    if(cur->state == connection_type::CLOSED) {
                        *cur = std::move(*(--end));
                        conn = cur;
                    } else ++count;
                }
            }
            connections().resize(end-connections().begin());
            return !count ? -1 : 0;
        }
        int connector::_south_pollout_handler(const south_type::handle_type& stream, event_mask& revents){
            const auto&[sfd, ssp] = stream;
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