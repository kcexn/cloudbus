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
#include "controller_connector.hpp"
#include <tuple>
#include <sys/un.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <cstring>
namespace cloudbus {
    namespace controller {
        namespace {
            template<class T>
            static bool owner_equal(const std::weak_ptr<T>& p1, const std::shared_ptr<T>& p2){
                return !p1.owner_before(p2) && !p2.owner_before(p1);
            }
            template<class T>
            static bool owner_equal(const std::shared_ptr<T>& p1, const std::weak_ptr<T>& p2){
                return !p1.owner_before(p2) && !p2.owner_before(p1);
            }
            template<class T>
            static bool owner_equal(const std::weak_ptr<T>& p1, const std::weak_ptr<T>& p2){
                return !p1.owner_before(p2) && !p2.owner_before(p1);
            }
        }
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
            using connection_type = connector::connection_type;
            int prev = conn.state;
            switch(conn.state){
                case connection_type::HALF_OPEN:
                    conn.timestamps[++conn.state] = time;
                case connection_type::OPEN:
                case connection_type::HALF_CLOSED:
                    if(type.op != messages::STOP)
                        break;
                    conn.timestamps[++conn.state] = time;
                    if(type.flags & messages::ABORT)
                        conn.timestamps[conn.state=connection_type::CLOSED] = time;
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
        static connector::events_type::iterator read_restart(
            int sockfd,
            connector::trigger_type& triggers,
            connector::events_type& events,
            connector::events_type::iterator& get,
            connector::events_type::iterator& put
        ){
            triggers.set(sockfd, POLLIN);
            auto it = std::find_if(get, events.end(),
                [&](const auto& e){
                    return e.fd == sockfd;
                }
            );
            if(it == events.end()) {
                auto goff=get-events.begin(), poff=put-events.begin();
                events.push_back({sockfd, POLLIN, POLLIN});
                put = events.begin()+poff;
                return events.begin()+goff;
            }
            it->revents |= POLLIN;
            return get;
        }
        connector::size_type connector::_handle(events_type& events){
            size_type handled = 0;
            auto put = events.begin();
            for(auto get = put; get != events.end(); ++get) {
                if(get->revents) {
                    auto sit = std::find_if(
                            south().begin(),
                            south().end(),
                        [&](auto& interface){
                            auto it = std::find_if(
                                    interface.streams().cbegin(),
                                    interface.streams().cend(),
                                [&](const auto& stream){
                                    const auto&[sockfd, ssp] = stream;
                                    if(sockfd==get->fd && get->revents & (POLLOUT | POLLERR))
                                        for(auto& c: connections())
                                            if(owner_equal(c.south, ssp))
                                                if(auto n = c.north.lock(); n && !n->eof())
                                                    get = read_restart(n->native_handle(), triggers(), events, get, put);
                                    return sockfd == get->fd;
                                }
                            );
                            if(it != interface.streams().cend())
                                handled += _handle(static_cast<south_type&>(interface), *it, get->revents);
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
                                        if(sockfd == get->fd && get->revents & (POLLOUT | POLLERR))
                                            for(auto& c: connections())
                                                if(owner_equal(c.north, nsp))
                                                    if(auto s = c.south.lock(); s && !s->eof())
                                                        get = read_restart(s->native_handle(), triggers(), events, get, put);
                                        return sockfd == get->fd;
                                    }
                                );
                                if(it != interface.streams().cend())
                                    handled += _handle(static_cast<north_type&>(interface), *it, get->revents);
                                return it != interface.streams().cend();
                            }
                        );
                    }
                    if(get->revents)
                        *put++ = std::move(*get);
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
                } else if(owner_equal(cur->north, np)) {
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
        int connector::_route(marshaller_type::north_format& buf, north_type& interface, const north_type::handle_type& stream, event_mask& revents){
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
                messages::msgheader abort = {
                    *eid, {1, sizeof(abort)},
                    {0,0},{messages::STOP, messages::ABORT}
                };
                auto conn=connections().begin(), end=connections().end();
                while(conn != end) {
                    if(conn->north.expired()) {
                        *conn = std::move(*(--end));
                    } else if (
                        !messages::uuidcmp_node(&conn->uuid, eid) &&
                        owner_equal(conn->south, ssp)
                    ){
                        if(auto n = conn->north.lock()) {
                            if(rem && !eof)
                                break;
                            if(conn->state == connection_type::CLOSED)
                                break;
                            if(conn->state == connection_type::HALF_CLOSED &&
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
                            auto prev = conn->state;
                            state_update(*conn, *type, time);
                            if(mode() == HALF_DUPLEX &&
                                    prev == connection_type::HALF_OPEN &&
                                    conn->state != connection_type::HALF_OPEN &&
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
                                        }
                                    }
                                }
                            }
                            buf.setstate(buf.eofbit);
                            return eof ? -1 : 0;
                        } else {
                            *conn = std::move(*(--end));
                        }
                    } else ++conn;
                }
                connections().resize(end-connections().begin());
                if(!rem) {
                    buf.setstate(buf.eofbit);
                    if(!eof && conn==connections().end() && !(type->flags & messages::ABORT)) {
                        ssp->write(reinterpret_cast<char*>(&abort), sizeof(abort));
                        triggers().set(ssp->native_handle(), POLLOUT);
                    }
                }
            }
            return eof ? -1 : 0;
        }
        static void erase_connect(
            interface_base& nbd,
            const connector::north_type::stream_ptr& nsp,
            interface_base& sbd,
            connector::south_type::handle_type& hnd,
            connector::trigger_type& triggers,
            connector::connections_type& connections
        ){
            auto conn=connections.begin(), end=connections.end();
            while(conn != end) {
                if(owner_equal(conn->north, nsp)) {
                    *conn = std::move(*(--end));
                } else ++conn;
            }
            connections.resize(end-connections.begin());
            auto it = std::find_if(
                    nbd.streams().begin(),
                    nbd.streams().end(),
                [&](const auto& hnd){
                    const auto&[fd, ptr] = hnd;
                    if(ptr == nsp)
                        triggers.clear(fd);
                    return ptr == nsp;
                }
            );
            nbd.erase(it);
            return (void)sbd.erase(hnd);
        }

        static const interface_base::handle_type& select_stream(interface_base& sbd) {
            constexpr std::size_t ratio = 2;
            const auto& measurements = metrics::get().streams().get_all_measurements();
            auto min = measurements.cbegin();
            auto ll = sbd.streams().begin();
            for(auto it=ll; it != sbd.streams().end(); ++it) {
                auto&[fd, sp] = *it;
                auto itm = std::find_if(
                        measurements.cbegin(), measurements.cend(),
                    [&](const auto& metric) {
                        return owner_equal(metric.wp, sp);
                    }
                );
                if(itm == measurements.cend())
                    return *it;
                if(itm->intercompletion <= itm->interarrival/ratio)
                    return *it;
                if(itm->intercompletion - itm->interarrival <
                    min->intercompletion - min->interarrival
                ){
                    min = itm;
                    ll = it;
                }
            }
            auto max_streams = std::max(sbd.addresses().size(), 1UL);
            if(sbd.streams().size() < max_streams)
                return sbd.make();
            return *ll;
        }

        std::streamsize connector::_north_connect(north_type& interface, const north_type::stream_ptr& nsp, marshaller_type::north_format& buf){
            constexpr std::size_t SHRINK_THRESHOLD = 4096;
            const auto n = connection_type::clock_type::now();
            auto eid = messages::make_uuid_v7();
            if(eid == messages::uuid{})
                return -1;
            metrics::get().arrivals() += 1;
            connections_type connect;
            for(auto& sbd: south()){
                auto&[sockfd, sptr] = select_stream(sbd);
                metrics::get().streams().add_arrival(sptr);
                if(sockfd != sptr->BAD_SOCKET) {
                    triggers().set(sockfd, POLLOUT);
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
                                return erase_connect(interface, nsp, sbd, hnd, triggers(), connections());
                            auto&[sockfd, sptr] = hnd;
                            if(sockfd == sptr->BAD_SOCKET) {
                                if( !(protocol == "TCP" || protocol == "UNIX") )
                                    return erase_connect(interface, nsp, sbd, hnd, triggers(), connections());
                                if( (sockfd = socket(addr->sa_family, SOCK_STREAM, 0)) == -1 )
                                    return erase_connect(interface, nsp, sbd, hnd, triggers(), connections());
                                if(protocol == "TCP") {
                                    int nodelay = 1;
                                    if(setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)))
                                        return erase_connect(interface, nsp, sbd, hnd, triggers(), connections());
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
                if(owner_equal(cur->north, nsp)) {
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
        int connector::_north_pollin_handler(north_type& interface, const north_type::handle_type& stream, event_mask& revents){
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
            const auto&[nfd, nsp] = stream;
            switch(session_state(connections(), nsp)){
                case connection_type::CLOSED:
                {
                    auto conn = connections().begin(), cur=conn, end=connections().end();
                    while((cur=conn) != end){
                        ++conn;
                        if(owner_equal(cur->north, nsp)) {
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
            const auto&[nfd, nsp] = stream;
            if(revents & POLLERR) {
                int ec = 0; socklen_t len = sizeof(ec);
                if(!getsockopt(nfd, SOL_SOCKET, SO_ERROR, &ec, &len) && ec)
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
            addresses.resize(end-addresses.begin());
            iface.addresses(std::move(addresses));
        }
        void connector::_south_err_handler(south_type& interface, const south_type::handle_type& stream, event_mask& revents){
            const auto&[sfd, ssp] = stream;
            const auto time = connection_type::clock_type::now();
            for(auto& conn: connections()) {
                if(owner_equal(conn.south, ssp)) {
                    if(auto n = conn.north.lock()) {
                        state_update(conn, {messages::STOP, 0}, time);
                        triggers().set(n->native_handle(), POLLOUT);
                    }
                }
            }
            revents = 0;
            triggers().clear(sfd);
            if(ssp->err() == ECONNREFUSED)
                expire_address_of(ssp, interface);
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
                return route(std::get<marshaller_type::south_format>(*it), interface, stream, revents);
            } else return -1;
        }
        int connector::_south_state_handler(const south_type::handle_type& stream){
            std::size_t count = 0;
            const auto& ssp = std::get<south_type::stream_ptr>(stream);
            auto conn = connections().begin(), cur=conn, end=connections().end();
            while((cur=conn) != end){
                ++conn;
                if(owner_equal(cur->south, ssp)) {
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
            if(revents & POLLERR) {
                int ec = 0; socklen_t len = sizeof(ec);
                if(!getsockopt(sfd, SOL_SOCKET, SO_ERROR, &ec, &len) && ec)
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