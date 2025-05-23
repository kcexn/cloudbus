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
#include "../../logging.hpp"
#include "../../metrics.hpp"
#include "controller_connector.hpp"
#include <tuple>
#include <sys/un.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <cstring>
namespace cloudbus {
    namespace controller {
        static constexpr std::streamsize MAX_BUFSIZE = 65536 * 4096; /* 256MiB */
        namespace {
            template<class T>
            static bool shrink_to_fit(std::vector<T>& vec){
                static constexpr std::size_t THRESH = 256;
                bool resized = false;
                const std::size_t capacity = vec.capacity();
                if( capacity > THRESH &&
                    vec.size() < capacity/8
                ){
                    vec  = std::vector<T>(
                        std::make_move_iterator(vec.begin()),
                        std::make_move_iterator(vec.end())
                    );
                    resized = true;
                }
                return resized;
            }
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
                    throw_system_error("Unable to set the socket to nonblocking mode.");
                return fd;
            }
            static int _accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen){
                int fd = 0;
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
                auto prev = conn.state;
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
                if(conn.state != prev && conn.state == connection_type::CLOSED)
                    metrics::get().streams().add_completion(conn.south);
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
        }
        connector::connector(
            trigger_type& triggers,
            const config::section& section
        ): Base(triggers, section) {}
        connector::size_type connector::_handle(events_type& events){
            size_type handled = 0;
            auto end = std::remove_if(
                    events.begin(), events.end(),
                [&](auto& ev) {
                    if(ev.revents) {
                        auto sit = std::find_if(
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
                        if(sit == south().end()){
                            std::find_if(
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
                        }
                    }
                    return !ev.revents;
                }
            );
            events.erase(end, events.end());
            return handled + Base::_handle(events);
        }
        static connector::connections_type::const_iterator write_prepare(
            connector::connections_type& connections,
            const connector::north_type::stream_ptr& np,
            const std::streamsize& tellp
        ){
            const std::streamsize pos = MAX_BUFSIZE-(tellp+sizeof(messages::msgheader));
            const auto cbegin=connections.cbegin(), cend=connections.cend();
            for(auto conn=cbegin; conn != cend; ++conn) {
                if(owner_equal(conn->north, np)) {
                    if(auto s = conn->south.lock()) {
                        if(s->fail())
                            continue;
                        if(s->tellp() >= pos && s->flush().bad())
                            continue;
                        if(s->tellp() > pos)
                            return conn;
                    }
                }
            }
            return cend;
        }
        int connector::_route(marshaller_type::north_format& buf, north_type& interface, const north_type::handle_type& stream, event_mask& revents){
            constexpr std::streamsize HDRLEN = sizeof(messages::msgheader);
            auto&[nsp, nfd] = stream;
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
                for(auto& conn: connections()) {
                    if(owner_equal(conn.north, nsp)) {
                        if(auto s = conn.south.lock()) {
                            if(++connected && conn.state != connection_type::CLOSED){
                                head.eid = conn.uuid;
                                s->write(reinterpret_cast<const char*>(&head), sizeof(head));
                                stream_write(*s, buf.seekg(0), p);
                                if(auto sockfd = s->native_handle(); sockfd != s->BAD_SOCKET)
                                    triggers().set(sockfd, POLLOUT);
                                state_update(conn, head.type, time);
                            }
                        }
                    }
                }
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
        int connector::_route(marshaller_type::south_format& buf, south_type& interface, const south_type::handle_type& stream, event_mask& revents){
            constexpr std::streamsize HDRLEN = sizeof(messages::msgheader);
            const auto&[ssp, sfd] = stream;
            const auto eof = ssp->eof();
            if(const auto *type = buf.type()){
                const std::streamsize pos=buf.tellp(), gpos=buf.tellg();
                if(const auto rem=buf.len()->length-pos; !rem) {
                    const auto *eid = buf.eid();
                    const std::streamsize seekpos =
                        (gpos <= HDRLEN)
                        ? HDRLEN
                        : gpos;
                    const auto time = connection_type::clock_type::now();
                    const messages::msgheader abort = {
                        *eid, {1, sizeof(abort)},
                        {0,0},{messages::STOP, messages::ABORT}
                    };
                    for(auto& conn: connections()) {
                        if (
                            !messages::uuidcmp_node(&conn.uuid, eid) &&
                            owner_equal(conn.south, ssp)
                        ){
                            if(conn.state == connection_type::CLOSED)
                                break;
                            if(auto n = conn.north.lock()) {
                                if(conn.state == connection_type::HALF_CLOSED &&
                                    !n->eof() && !(type->flags & messages::ABORT)
                                ){
                                    break;
                                }
                                triggers().set(n->native_handle(), POLLOUT);
                                if(pos > seekpos){
                                    buf.seekg(seekpos);
                                    if(!_south_write(n, buf))
                                        return clear_triggers(sfd, triggers(), revents, (POLLIN | POLLHUP));
                                }
                                auto prev = conn.state;
                                state_update(conn, *type, time);
                                if(mode() == HALF_DUPLEX &&
                                        prev == connection_type::HALF_OPEN &&
                                        conn.state != connection_type::HALF_OPEN &&
                                        seekpos == HDRLEN && pos > seekpos
                                ){
                                    for(auto& c: connections()){
                                        if(c.uuid == *eid && c.state < connection_type::HALF_CLOSED) {
                                            if(!owner_equal(c.south, ssp)) {
                                                if(auto sp = c.south.lock()) {
                                                    sp->write(reinterpret_cast<const char*>(&abort), sizeof(abort));
                                                    triggers().set(sp->native_handle(), POLLOUT);
                                                    state_update(c, abort.type, time);
                                                }
                                                Logger::getInstance().debug(
                                                    __FILE__ " -- abort connection and latch."
                                                );
                                            }
                                        }
                                    }
                                }
                            }
                            buf.setstate(buf.eofbit);
                            return eof ? -1 : 0;
                        }
                    }
                    buf.setstate(buf.eofbit);
                    if(!eof && !(type->flags & messages::ABORT)) {
                        ssp->write(reinterpret_cast<const char*>(&abort), sizeof(abort));
                        triggers().set(ssp->native_handle(), POLLOUT);
                    }
                }
            }
            return eof ? -1 : 0;
        }
        static void erase_connect(
            interface_base& nbd,
            const connector::north_type::stream_ptr& nsp,
            connector::south_type::handle_type& hnd,
            connector::trigger_type& triggers,
            connector::connections_type& connections
        ){
            auto&[ssp, sfd] = hnd;
            auto begin = connections.begin(), end = connections.end();
            auto new_end = std::remove_if(
                    begin,
                    end,
                [&](auto& conn) {
                    if(owner_equal(conn.south, ssp)) {
                        if(sfd != ssp->BAD_SOCKET)
                            triggers.set(sfd, POLLOUT);
                    }
                    return owner_equal(conn.south, ssp);
                }
            );
            connections.erase(new_end, end);
            auto it = std::find_if(
                    nbd.streams().begin(),
                    nbd.streams().end(),
                [&](const auto& hnd){
                    const auto&[ptr, fd] = hnd;
                    if(ptr == nsp)
                        triggers.clear(fd);
                    return ptr == nsp;
                }
            );
            return (void)nbd.erase(it);
        }
        static std::size_t rescale(const std::size_t& size) {
            if(size == 3)
                return 4;
            const auto half = size/2;
            return half <= SIZE_MAX/3 ?
                std::max(3*half, 1UL) :
                size;
        }
        static const interface_base::handle_type& select_stream(interface_base& sbd) {
            using stream_ptr = interface_base::stream_ptr;
            static constexpr std::size_t ratio = 2;
            const auto num_streams = sbd.streams().size();
            if(!num_streams)
                return sbd.make();
            const auto& measurements = metrics::get().streams().get_all_measurements();
            std::size_t loaded = 0;
            auto mbegin = measurements.cbegin(), mend = measurements.cend();
            auto min = mbegin;
            auto lru = sbd.streams().begin(), end=sbd.streams().end();
            for(auto it=lru; it != end; ++it) {
                auto&[sp, fd] = *it;
                auto lb = std::lower_bound(
                    mbegin,
                    mend,
                    sp,
                    [](const auto& lhs, const stream_ptr& sp) {
                        return lhs.wp.owner_before(sp);
                    }
                );
                /* return the stream if there are no associated metrics. */
                if(lb == mend || !owner_equal(lb->wp, sp))
                    return *it;
                /* count number of loaded nodes. */
                if(lb->intercompletion > lb->interarrival/ratio)
                    ++loaded;
                /* least recently used */
                if(lb->last_arrival < min->last_arrival) {
                    min = lb;
                    lru = it;
                }
            }
            if(loaded >= num_streams) {
                /* Try and scale-out before applying LRU. It is         *
                 * possible for multiple streams to be assigned the     *
                 * same destination address if DNS TTL's have expired,  *
                 * or if we have opened and closed streams enough for   *
                 * DNS round-robin host selection to have wrapped       *
                 * around on itself. rescale() adds extra streams so    *
                 * that even with duplicates we will eventually load    *
                 * balance across all possible backend hosts.           */
                if(num_streams < rescale(sbd.addresses().size()))
                    return sbd.make();
            }
            return *lru;
        }
        std::streamsize connector::_north_connect(
            north_type& interface,
            const north_type::stream_ptr& nsp,
            marshaller_type::north_format& buf
        ){
            const auto n = connection_type::clock_type::now();
            auto eid = messages::make_uuid_v7();
            if(eid == messages::uuid{})
                return -1;
            metrics::get().arrivals().fetch_add(1, std::memory_order_relaxed);
            connections_type connect;
            for(auto& sbd: south()) {
                auto&[sptr, sockfd] = select_stream(sbd);
                metrics::get().streams().add_arrival(sptr);
                if(sockfd != sptr->BAD_SOCKET) {
                    triggers().set(sockfd, POLLIN | POLLOUT);
                    sptr->clear(sptr->rdstate() & ~sptr->failbit);
                } else {
                    sbd.register_connect(
                        sptr,
                        [&](
                            auto& hnd,
                            const auto *addr,
                            auto addrlen,
                            const std::string& protocol
                        ){
                            if(addr == nullptr)
                                return erase_connect(interface, nsp, hnd, triggers(), connections());
                            auto&[sptr, sockfd] = hnd;
                            if(sockfd == sptr->BAD_SOCKET) {
                                if( !(protocol == "TCP" || protocol == "UNIX") )
                                    return erase_connect(interface, nsp, hnd, triggers(), connections());
                                if( (sockfd = socket(addr->sa_family, SOCK_STREAM, 0)) == -1 )
                                    return erase_connect(interface, nsp, hnd, triggers(), connections());
                                if(protocol == "TCP") {
                                    int nodelay = 1;
                                    if(setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)))
                                        return erase_connect(interface, nsp, hnd, triggers(), connections());
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
                }
                if(mode() == FULL_DUPLEX){
                    if((eid.clock_seq_reserved & messages::CLOCK_SEQ_MAX) == messages::CLOCK_SEQ_MAX)
                        eid.clock_seq_reserved &= ~messages::CLOCK_SEQ_MAX;
                    else ++eid.clock_seq_reserved;
                }
                connect.push_back(connection_type::make(
                    eid,
                    nsp,
                    sptr,
                    nsp->eof() ?
                        connection_type::HALF_CLOSED :
                        connection_type::HALF_OPEN,
                    n
                ));
            }
            const std::streamsize pos = buf.tellp();
            messages::msgheader head;
            head.len = {1, static_cast<std::uint16_t>(pos + sizeof(head))};
            head.version = {0,0};
            head.type = (nsp->eof()) ?
                messages::msgtype{messages::STOP, messages::INIT} :
                messages::msgtype{messages::DATA, messages::INIT};
            std::streamsize len = 0;
            if(write_prepare(connect, nsp, pos) == connect.cend()){
                for(auto& c: connect){
                    if(auto s = c.south.lock()){
                        head.eid = c.uuid;
                        s->write(reinterpret_cast<const char*>(&head), sizeof(head));
                        stream_write(*s, buf.seekg(0), pos);
                    }
                }
                len = sizeof(head) + pos;
            }
            connections().insert(
                connections().end(),
                std::make_move_iterator(connect.begin()),
                std::make_move_iterator(connect.end())
            );
            shrink_to_fit(connections());
            return len;
        }
        void connector::_north_err_handler(north_type& interface, const north_type::handle_type& stream, event_mask& revents){
            auto begin = connections().begin(), end = connections().end();
            messages::msgheader abort = {
                {}, {1, static_cast<std::uint16_t>(sizeof(abort))},
                {0,0}, {messages::STOP, messages::ABORT}
            };
            const auto&[nsp, nfd] = stream;
            auto new_end = std::remove_if(
                    begin,
                    end,
                [&](auto& conn) {
                    if(owner_equal(conn.north, nsp)) {
                        if(auto s = conn.south.lock()) {
                            triggers().set(s->native_handle(), POLLOUT);
                            if(conn.state < connection_type::CLOSED) {
                                abort.eid = conn.uuid;
                                s->write(reinterpret_cast<char*>(&abort), sizeof(abort));
                            }
                        }
                    }
                    return owner_equal(conn.north, nsp);
                }
            );
            connections().erase(new_end, end);
            revents = 0;
            triggers().clear(nfd);
            interface.erase(stream);
        }
        int connector::_north_pollin_handler(north_type& interface, const north_type::handle_type& stream, event_mask& revents){
            auto it = marshaller().unmarshal(stream);
            if(std::get<north_type::stream_ptr>(stream)->gcount() == 0)
                revents &= ~(POLLIN | POLLHUP);
            return route(*it->pbuf, interface, stream, revents);
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
                    static constexpr int nodelay = 1;
                    if(setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)))
                        throw_system_error("Unable to set TCP_NODELAY socket option.");
                }
                triggers().set(sockfd, POLLIN);
            }
            revents &= ~(POLLIN | POLLHUP);
            return (sockfd == -EWOULDBLOCK) ? 0 : -1;
        }
        static auto session_state(const connector::connections_type& connections, const connector::north_type::stream_ptr& nsp){
            using connection = connector::connection_type;
            short state = connection::CLOSED;
            for(const auto& c: connections) {
                if(owner_equal(c.north, nsp)) {
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
            const auto&[nsp, nfd] = stream;
            switch(session_state(connections(), nsp)) {
                case connection_type::CLOSED:
                    return _north_err_handler(interface, stream, revents);
                case connection_type::HALF_CLOSED:
                    revents |= POLLHUP;
                    shutdown(nfd, SHUT_WR);
                default: break;
            }
        }
        int connector::_north_pollout_handler(const north_type::handle_type& stream, event_mask& revents){
            const auto&[nsp, nfd] = stream;
            if(revents & POLLERR)
            {
                int ec = 0; socklen_t len = sizeof(ec);
                if(getsockopt(nfd, SOL_SOCKET, SO_ERROR, &ec, &len))
                    throw_system_error("Unable to get SO_ERROR.");
                if(ec)
                    nsp->err() = ec;
                Logger::getInstance().error(
                    "northbound socket stream error: " +
                    std::system_category().message(nsp->err())
                );
            }
            if(revents & (POLLERR | POLLNVAL))
                nsp->setstate(nsp->badbit);
            if(nsp->flush().fail())
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
                if(_north_pollout_handler(stream, revents)) {
                    _north_err_handler(interface, stream, revents);
                } else {
                    _north_state_handler(interface, stream, revents);
                }
            }
            if(revents & (POLLIN | POLLHUP)){
                ++handled;
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
            auto begin = addresses.begin(), end = addresses.end();
            auto new_end = std::remove_if(
                    begin,
                    end,
                [&](auto& a){
                    const auto&[addr_, addrlen_, ttl_, weight_] = a;
                    const auto&[time, interval] = ttl_;
                    return interval.count() > -1 &&
                        addrlen_ == addrlen &&
                        !std::memcmp(&addr, &addr_, addrlen);
                }
            );
            addresses.erase(new_end, end);
            iface.addresses(std::move(addresses));
        }
        void connector::_south_err_handler(south_type& interface, const south_type::handle_type& stream, event_mask& revents){
            const auto&[ssp, sfd] = stream;
            const auto time = connection_type::clock_type::now();
            auto begin = connections().begin(), end = connections().end();
            auto new_end = std::remove_if(
                    begin,
                    end,
                [&](auto& conn) {
                    if(owner_equal(conn.south, ssp)) {
                        if(auto n = conn.north.lock())
                        {
                            state_update(conn, {messages::STOP, messages::ABORT}, time);
                            triggers().set(n->native_handle(), POLLOUT);
                        }
                        else return true;
                    }
                    return false;
                }
            );
            connections().erase(new_end, end);
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
        int connector::_south_pollin_handler(south_type& interface, const south_type::handle_type& stream, event_mask& revents){
            if(auto it = marshaller().marshal(stream); it != marshaller().south().end()){
                if(std::get<south_type::stream_ptr>(stream)->gcount() == 0)
                    revents &= ~(POLLIN | POLLHUP);
                return route(*it->pbuf, interface, stream, revents);
            } else return -1;
        }
        int connector::_south_state_handler(const south_type::handle_type& stream){
            const auto&[ssp, sfd] = stream;
            for(const auto& conn: connections()) {
                if(owner_equal(conn.south, ssp) &&
                    conn.state < connection_type::CLOSED
                ){
                    return 0;
                }
            }
            using milliseconds = std::chrono::milliseconds;
            using weak_ptr = std::weak_ptr<south_type::stream_type>;
            triggers().clear(sfd);
            timeouts().addEvent(
                ssp,
                clock_type::now()+milliseconds(30000),
                [&, wp=weak_ptr(ssp)]() {
                    if(!wp.expired()) {
                        auto begin = connections().begin(), end = connections().end();
                        auto it = std::find_if(
                            begin,
                            end,
                            [&](const auto& conn) {
                                return owner_equal(conn.south, wp) &&
                                    conn.state < connection_type::CLOSED;
                            }
                        );
                        if(it == end) {
                            if(auto sp = wp.lock()) {
                                sp->setstate(sp->failbit);
                                triggers().set(sp->native_handle(), POLLOUT);
                            }
                        }
                    }
                }
            );
            return 0;
        }
        int connector::_south_pollout_handler(const south_type::handle_type& stream, event_mask& revents){
            const auto&[ssp, sfd] = stream;
            if(revents & POLLERR)
            {
                int ec = 0; socklen_t len = sizeof(ec);
                if(getsockopt(sfd, SOL_SOCKET, SO_ERROR, &ec, &len))
                    throw_system_error("Unable to get SO_ERROR.");
                if(ec)
                    ssp->err() = ec;
                Logger::getInstance().error(
                    "southbound socket stream error: " +
                    std::system_category().message(ssp->err())
                );
            }
            if(revents & (POLLERR | POLLNVAL))
                ssp->setstate(ssp->badbit);
            if(ssp->flush().fail())
                return -1;
            if(ssp->tellp() == 0)
                triggers().clear(sfd, POLLOUT);
            revents &= ~(POLLOUT | POLLERR | POLLNVAL);
            return 0;
        }
        connector::size_type connector::_handle(south_type& interface, const south_type::handle_type& stream, event_mask& revents){
            size_type handled = 0;
            if( (revents & (POLLOUT | POLLERR | POLLNVAL)) && ++handled )
            {
                if(_south_pollout_handler(stream, revents))
                    _south_err_handler(interface, stream, revents);
                else if(_south_state_handler(stream))
                    _south_err_handler(interface, stream, revents);
            }
            if( (revents & (POLLIN | POLLHUP)) && ++handled)
            {
                if(_south_pollin_handler(interface, stream, revents))
                    _south_err_handler(interface, stream, revents);
            }
            return handled;
        }
    }
}
