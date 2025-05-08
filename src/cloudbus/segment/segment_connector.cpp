/*
*   Copyright 2025 Kevin Exton
*   This file is part of Cloudbus.
*
*   Cloudbus is free software: you can redistribute it and/or modify it under the
*   terms of the GNU General Public License as published by the Free Software
*   Foundation, either version 3 of the License, or any later version.
*
*   Cloudbus is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
*   without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*   See the GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License along with Cloudbus.
*   If not, see <https://www.gnu.org/licenses/>.
*/
#include "../../metrics.hpp"
#include "segment_connector.hpp"
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <cstring>
namespace cloudbus{
    namespace segment {
        static constexpr std::streamsize MAX_BUFSIZE = 65536 * 4096; /* 256MiB */
        static void throw_system_error(const std::string& what) {
            throw std::system_error(
                std::error_code(errno, std::system_category()),
                what
            );
        }
        static int set_flags(int fd){
            int flags = 0;
            if(fcntl(fd, F_SETFD, FD_CLOEXEC))
                throw_system_error("Unable to set the cloexec flag.");
            if((flags = fcntl(fd, F_GETFL)) == -1)
                throw_system_error("Unable to get file flags.");
            if(fcntl(fd, F_SETFL, flags | O_NONBLOCK))
                throw_system_error("Unable to set O_NONBLOCK.");
            return fd;
        }
        static int _accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen){
            int fd = -1;
            while( (fd = accept(sockfd, addr, addrlen)) < 0 ){
                switch(errno){
                    case EINTR:
                        continue;
                    default:
                        return -errno;                    
                }
            }
            return set_flags(fd);
        }
        static void state_update(
            connector::connection_type& conn,
            const messages::msgtype& type,
            const connector::connection_type::time_point time
        ){
            using connection_type = connector::connection_type;
            switch(conn.state){
                case connection_type::HALF_OPEN:
                    conn.timestamps->at(++conn.state) = time;
                case connection_type::OPEN:
                case connection_type::HALF_CLOSED:
                    if(type.op != messages::STOP)
                        break;
                    conn.timestamps->at(++conn.state) = time;
                    if(type.flags & messages::ABORT)
                        conn.timestamps->at(conn.state=connection_type::CLOSED) = time;
                default:
                    break;
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
        connector::size_type connector::_handle(events_type& events){
            size_type handled = 0;
            auto end = std::remove_if(
                    events.begin(), events.end(),
                [&](auto& ev) {
                    if(ev.revents) {
                        auto nit = std::find_if(
                                north().begin(),
                                north().end(),
                            [&](auto& interface){
                                auto it = std::find_if(
                                        interface.streams().cbegin(),
                                        interface.streams().cend(),
                                    [&](const auto& stream){
                                        const auto&[nsp, sockfd] = stream;
                                        if(sockfd == ev.fd && ev.revents & (POLLOUT | POLLERR))
                                            for(auto& c: connections())
                                                if(owner_equal(c.north, nsp))
                                                    if(auto s = c.south.lock(); s && !s->eof())
                                                        triggers().set(s->native_handle(), POLLIN);
                                        return sockfd == ev.fd;
                                    }
                                );
                                if(it != interface.streams().cend())
                                    handled += _handle(static_cast<north_type&>(interface), *it, ev.revents);
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
                                            const auto&[ssp, sockfd] = stream;
                                            if(sockfd==ev.fd && ev.revents & (POLLOUT | POLLERR))
                                                for(auto& c: connections())
                                                    if(owner_equal(c.south, ssp))
                                                        if(auto n = c.north.lock(); n && !n->eof())
                                                            triggers().set(n->native_handle(), POLLIN);
                                            return sockfd == ev.fd;
                                        }
                                    );
                                    if(it != interface.streams().cend())
                                        handled += _handle(static_cast<south_type&>(interface), *it, ev.revents);
                                    return it != interface.streams().cend();
                                }
                            );
                        }
                    }
                    return !ev.revents;
                }
            );
            events.erase(end, events.end());
            return handled + Base::_handle(events);
        }
        int connector::_route(marshaller_type::north_format& buf, north_type& interface, const north_type::handle_type& stream, event_mask& revents){
            constexpr std::streamsize HDRLEN = sizeof(messages::msgheader);
            const auto&[nsp, nfd] = stream;
            const auto eof = nsp->eof();
            if(const auto *type = buf.type()) {
                const std::streamsize pos=buf.tellp(), gpos=buf.tellg();
                if(const auto rem=buf.len()->length-pos; !rem) {
                    const auto *eid = buf.eid();
                    const std::streamsize seekpos =
                        (gpos <= HDRLEN)
                            ? HDRLEN
                            : gpos;
                    const auto time = connection_type::clock_type::now();
                    for(auto& conn: connections()) {
                        if(conn.uuid == *eid && owner_equal(conn.north, nsp)) {
                            if(conn.state == connection_type::CLOSED)
                                break;
                            if(auto s = conn.south.lock()) {
                                auto sockfd = s->native_handle();
                                if(sockfd != s->BAD_SOCKET)
                                    triggers().set(sockfd, POLLOUT);
                                if(pos > seekpos) {
                                    buf.seekg(seekpos);
                                    if(!_north_write(s, buf))
                                        return clear_triggers(nfd, triggers(), revents, (POLLIN | POLLHUP));
                                }
                                if(type->flags & messages::ABORT)
                                    s->setstate(s->badbit); 
                            }
                            state_update(conn, *type, time);
                            buf.setstate(buf.eofbit);
                            return eof ? -1 : 0;
                        }
                    }
                    buf.setstate(buf.eofbit);
                    if(!eof) {
                        if( (type->flags & messages::INIT) &&
                            pos > HDRLEN
                        ){
                            buf.seekg(HDRLEN);
                            if(auto status = north_connect(interface, nsp, buf)) {
                                if(status < 0)
                                    return -1;
                            } else return clear_triggers(nfd, triggers(), revents, (POLLIN | POLLHUP));
                        }
                        else if( !(type->flags & messages::ABORT) )
                        {
                            messages::msgheader abort{
                                *buf.eid(), {1, sizeof(abort)},
                                {0,0}, {messages::STOP, messages::ABORT}
                            };
                            nsp->write(reinterpret_cast<char*>(&abort), sizeof(abort));
                            triggers().set(nfd, POLLOUT);
                        }
                    }
                }
            }
            return eof ? -1 : 0;
        }
        int connector::_route(marshaller_type::south_format& buf, south_type& interface, const south_type::handle_type& stream, event_mask& revents){
            using clock_type = connection_type::clock_type;
            auto&[ssp, sfd] = stream;
            const auto p = buf.tellp();
            const auto eof = ssp->eof();
            for(auto& conn: connections()) {
                if( owner_equal(conn.south, ssp) ) {
                    if(auto n = conn.north.lock()) {
                        messages::msgtype t = {messages::DATA, 0};
                        if(eof)
                            t.op = messages::STOP;
                        if(eof || p) {
                            buf.seekg(0);
                            triggers().set(n->native_handle(), POLLOUT);
                            if(!_south_write(n, conn, buf))
                                return clear_triggers(sfd, triggers(), revents, (POLLIN | POLLHUP));
                        }
                        state_update(conn, t, clock_type::now());
                        if(eof)
                            triggers().clear(sfd, POLLIN);
                        return conn.state == conn.CLOSED ? -1 : 0;
                    }
                }
            }
            return p ? -1 : 0;
        }
        static void erase_connect(
            const connector::north_type::stream_ptr& nsp,
            interface_base& sbd,
            connector::south_type::handle_type& hnd,
            connector::trigger_type& triggers,
            connector::connections_type& connections
        ){
            messages::msgheader abort {
                {}, {1, sizeof(abort)},
                {0,0}, {messages::STOP, messages::ABORT}
            };
            auto&[ssp, sfd] = hnd;
            auto end = std::remove_if(
                    connections.begin(),
                    connections.end(),
                [&](auto& conn) {
                    if(owner_equal(conn.south, ssp)) {
                        abort.eid = conn.uuid;
                        nsp->write(reinterpret_cast<char*>(&abort), sizeof(abort));
                        triggers.set(nsp->native_handle(), POLLOUT);
                    }
                    return owner_equal(conn.south, ssp);
                }
            );
            connections.erase(end, connections.end());
            return (void)sbd.erase(hnd);
        }
        std::streamsize connector::_north_connect(north_type& interface, const north_type::stream_ptr& nsp, marshaller_type::north_format& buf){
            auto& sbd = south().front();
            auto&[ssp, sfd] = sbd.make();
            metrics::get().arrivals().fetch_add(1, std::memory_order_relaxed);
            sbd.register_connect(
                ssp,
                [&](
                    auto& hnd,
                    const auto *addr,
                    auto addrlen,
                    const std::string& protocol
                ){
                    if(addr == nullptr)
                        return erase_connect(nsp, sbd, hnd, triggers(), connections());
                    auto&[sptr, sockfd] = hnd;
                    if(sockfd == sptr->BAD_SOCKET) {
                        if( !(protocol == "TCP" || protocol == "UNIX") )
                            return erase_connect(nsp, sbd, hnd, triggers(), connections());
                        if( (sockfd = socket(addr->sa_family, SOCK_STREAM, 0)) == -1 )
                            return erase_connect(nsp, sbd, hnd, triggers(), connections());
                        if(protocol == "TCP") {
                            static constexpr int nodelay = 1;
                            if(setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)))
                                return erase_connect(nsp, sbd, hnd, triggers(), connections());
                        }
                        sptr->native_handle() = set_flags(sockfd);
                        sptr->connectto(addr, addrlen);
                    }
                    triggers().set(sockfd, POLLIN | POLLOUT);
                }
            );
            /* Address resolution only on the first pending connect. */
            if(sbd.addresses().empty() && sbd.npending()==1)
                resolver().resolve(sbd);
            connections().push_back(
                connection_type::make(
                    *buf.eid(),
                    nsp,
                    ssp,
                    buf.type()->op == messages::STOP ?
                        connection_type::HALF_CLOSED :
                        connection_type::HALF_OPEN
                )
            );
            static constexpr std::size_t THRESH = 32;
            if( connections().size() > THRESH &&
                connections().size() < connections().capacity()/8
            ){
                connections() = connections_type(
                    std::make_move_iterator(connections().begin()),
                    std::make_move_iterator(connections().end())
                );
            }
            return _north_write(ssp, buf);
        }
        void connector::_north_err_handler(north_type& interface, const north_type::handle_type& stream, event_mask& revents){
            const auto time = connection_type::clock_type::now();
            const auto&[nsp, nfd] = stream;
            auto end = std::remove_if(
                    connections().begin(),
                    connections().end(),
                [&](auto& conn) {
                    if(owner_equal(conn.north, nsp)) {
                        if(auto s = conn.south.lock())
                        {
                            state_update(conn, {messages::STOP, messages::ABORT}, time);
                            triggers().set(s->native_handle(), POLLOUT);
                        }
                        else return true;
                    }
                    return false;
                }
            );
            connections().erase(end, connections().end());
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
        int connector::_north_pollin_handler(north_type& interface, const north_type::handle_type& stream, event_mask& revents){
            if(auto it = marshaller().unmarshal(stream); it != marshaller().north().end()){
                using stream_ptr = north_type::stream_ptr;
                if(!std::get<stream_ptr>(stream)->gcount())
                    revents &= ~(POLLIN | POLLHUP);
                return route(*it->pbuf, interface, stream, revents);
            } else return -1;
        }
        int connector::_north_accept_handler(north_type& interface, const north_type::handle_type& stream, event_mask& revents){
            using native_handle_type = north_type::native_handle_type;
            if(drain())
                return -1;
            int sockfd = -1, listenfd = std::get<native_handle_type>(stream);
            while( (sockfd = _accept(listenfd, nullptr, nullptr)) > -1 ){
                interface.make(sockfd, true);
                if(interface.protocol() == "TCP") {
                    static constexpr int nodelay = 1;
                    if(setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)))
                        throw_system_error("Unable to set TCP_NODELAY.");
                }
                triggers().set(sockfd, POLLIN);
            }
            revents &= ~(POLLIN | POLLHUP);
            return (sockfd == -EWOULDBLOCK) ? 0 : -1;
        }
        int connector::_north_pollout_handler(const north_type::handle_type& stream, event_mask& revents){
            auto&[nsp, nfd] = stream;
            if(revents & POLLERR)
            {
                int ec = 0; socklen_t len = sizeof(ec);
                if(getsockopt(nfd, SOL_SOCKET, SO_ERROR, &ec, &len))
                    throw_system_error("Unable to get SO_ERROR.");
                if(ec)
                    nsp->err() = ec;
            }
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
            if( (revents & (POLLOUT | POLLERR | POLLNVAL)) && ++handled )
            {
                if(_north_pollout_handler(stream, revents))
                    _north_err_handler(interface, stream, revents);
            }
            if( (revents & (POLLIN | POLLHUP)) && ++handled ){
                if(stream == interface.streams().front()) {
                    if(_north_accept_handler(interface, stream, revents))
                        _north_err_handler(interface, stream, revents);
                } else {
                    if(_north_pollin_handler(interface, stream, revents))
                        _north_err_handler(interface, stream, revents);
                }
            }
            return handled;
        }
        static void expire_address_of(const interface_base::stream_ptr& sptr, interface_base& iface) {
            const auto&[addr, addrlen] = sptr->sendbuf()->data;
            auto addresses = iface.addresses();
            auto end = std::remove_if(
                    addresses.begin(),
                    addresses.end(),
                [&](auto& a){
                    const auto&[addr_, addrlen_, ttl_, weight_] = a;
                    const auto&[time, interval] = ttl_;
                    return interval.count() > -1 &&
                        addrlen_ == addrlen &&
                        !std::memcmp(&addr, &addr_, addrlen);
                }
            );
            addresses.erase(end, addresses.end());
            iface.addresses(std::move(addresses));
        }        
        void connector::_south_err_handler(south_type& interface, const south_type::handle_type& stream, event_mask& revents){
            const auto&[ssp, sfd] = stream;
            messages::msgheader abort{
                {}, {1, sizeof(abort)},
                {0,0}, {messages::STOP, messages::ABORT}
            };            
            auto end = std::remove_if(
                    connections().begin(),
                    connections().end(),
                [&](auto& conn) {
                    if(owner_equal(conn.south, ssp)) {
                        if(auto n = conn.north.lock()) {
                            if(conn.state < connection_type::CLOSED) {
                                abort.eid = conn.uuid;
                                n->write(reinterpret_cast<char*>(&abort), sizeof(abort));
                                triggers().set(n->native_handle(), POLLOUT);
                            }
                        }
                    }
                    return owner_equal(conn.south, ssp);
                }
            );
            connections().erase(end, connections().end());
            revents = 0;
            triggers().clear(sfd);
            switch(ssp->err()) {
                case ECONNREFUSED:
                    expire_address_of(ssp, interface);
                default:
                    break;
            }
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
        int connector::_south_pollin_handler(south_type& interface, const south_type::handle_type& stream, event_mask& revents){
            if(auto it = marshaller().marshal(stream); it != marshaller().south().end()){
                using stream_ptr = south_type::stream_ptr;
                if(!std::get<stream_ptr>(stream)->gcount())
                    revents &= ~(POLLIN | POLLHUP);
                return route(*it->pbuf, interface, stream, revents);
            } else return -1;
        }
        void connector::_south_state_handler(south_type& interface, const south_type::handle_type& stream, event_mask& revents){
            const auto&[ssp, sfd] = stream;
            for(auto& conn: connections()) {
                if(owner_equal(conn.south, ssp)) {
                    switch(conn.state) {
                        case connection_type::CLOSED:
                            return _south_err_handler(interface, stream, revents);
                        case connection_type::HALF_CLOSED:
                            revents |= POLLHUP;
                            shutdown(sfd, SHUT_WR);
                        default:
                            return;
                    }
                }
            }
        }
        int connector::_south_pollout_handler(const south_type::handle_type& stream, event_mask& revents){
            auto&[ssp, sfd] = stream;
            if(revents & POLLERR)
            {
                int ec = 0; socklen_t len = sizeof(ec);
                if(getsockopt(sfd, SOL_SOCKET, SO_ERROR, &ec, &len))
                    throw_system_error("Unable to get SO_ERROR.");
                if(ec)
                    ssp->err() = ec;
            }
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
            if( (revents & (POLLOUT | POLLERR | POLLNVAL)) && ++handled ) {
                if(_south_pollout_handler(stream, revents))
                    _south_err_handler(interface, stream, revents);
                else _south_state_handler(interface, stream, revents);
            }
            if( (revents & (POLLIN | POLLHUP)) && ++handled ) {
                if(_south_pollin_handler(interface, stream, revents))
                    _south_err_handler(interface, stream, revents);
            }
            return handled;
        }
    }
}