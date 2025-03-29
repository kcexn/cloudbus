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
        static connector::connections_type::iterator write_prepare(
            connector::connections_type& connections,
            connector::trigger_type& triggers,
            const connector::north_type::stream_ptr& np,
            const std::streamsize& tellp
        ){
            const std::streamsize pos = MAX_BUFSIZE-(tellp+sizeof(messages::msgheader));
            for(auto conn = connections.begin(); conn < connections.end(); ++conn){
                if(auto n = conn->north.lock(); n && n==np){
                    if(auto s = conn->south.lock()){
                        if(s->fail())
                            continue;
                        if(s->tellp() >= pos &&
                                s->flush().bad())
                            continue;
                        if(s->tellp() > pos)
                            return conn;
                    } else conn = --connections.erase(conn);
                }
            }
            return connections.end();
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
            if(it != events.end()){
                it->revents |= POLLIN;
                return ev;
            }
            auto off = ev - events.begin();
            events.emplace_back(connector::event_type{sockfd, POLLIN, POLLIN});
            return events.begin() + off;
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
                                    const auto&[sockfd, nsp] = stream;
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
                                    const auto&[sockfd, ssp] = stream;
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
        int connector::_route(marshaller_type::north_format& buf, const north_type& interface, const north_type::handle_type& stream, event_mask& revents){
            constexpr std::streamsize HDRLEN = sizeof(messages::msgheader);
            auto&[nfd, nsp] = stream;
            const auto eof = nsp->eof();
            if(const auto p = buf.tellp(); eof || p > 0){
                messages::msgheader head;
                head.len = messages::msglen{1, static_cast<std::uint16_t>(p+HDRLEN)};
                head.version = messages::msgversion{0,0};
                head.type = messages::msgtype{eof ? messages::STOP : messages::DATA, 0};
                if(write_prepare(connections(), triggers(), nsp, p) != connections().end())
                    return clear_triggers(nfd, triggers(), revents, (POLLIN | POLLHUP));
                const auto time = connection_type::clock_type::now();
                std::size_t connected = 0;
                for(auto conn=connections().begin(); conn < connections().end(); ++conn){
                    if(auto n = conn->north.lock(); n && n==nsp){
                        if(auto s = conn->south.lock()){
                            if(++connected && conn->state != connection_type::CLOSED){
                                state_update(*conn, head.type, time);
                                head.eid = conn->uuid;
                                s->write(reinterpret_cast<const char*>(&head), sizeof(head));
                                stream_write(*s, buf.seekg(0), p);
                                triggers().set(s->native_handle(), POLLOUT);
                            }
                        } else conn = --connections().erase(conn);
                    }
                }
                if(!eof && !connected){
                    if(auto status = north_connect(interface, nsp, buf)){
                        if(status < 0)
                            return -1;
                    } else return clear_triggers(nfd, triggers(), revents, (POLLIN | POLLHUP));
                }
            }
            if(eof)
                triggers().clear(nfd, POLLIN);
            return 0;
        }
        int connector::_route(marshaller_type::south_format& buf, const south_type& interface, const south_type::handle_type& stream, event_mask& revents){
            constexpr std::streamsize HDRLEN = sizeof(messages::msgheader);
            auto&[sfd, ssp] = stream;
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
                for(auto conn = connections().begin(); conn < connections().end(); ++conn){
                    if(auto s = conn->south.lock();
                            !messages::uuid_cmpnode(&conn->uuid, eid) &&
                            s && s == ssp
                    ){
                        if(auto n = conn->north.lock()){
                            if(conn->state==connection_type::HALF_CLOSED &&
                                    !n->eof() && !(type->flags & messages::ABORT)
                            ){
                                break;
                            }
                            if(conn->state == connection_type::CLOSED){
                                if(type->op == messages::STOP)
                                    conn = --connections().erase(conn);
                                break;
                            }
                            if(mode()==FULL_DUPLEX && rem && !eof)
                                break;
                            triggers().set(n->native_handle(), POLLOUT);
                            if(pos > seekpos){
                                buf.seekg(seekpos);
                                if(!_south_write(n, buf))
                                    return clear_triggers(sfd, triggers(), revents, (POLLIN | POLLHUP));
                            }
                            auto prev = conn->state;
                            if(!rem){
                                state_update(*conn, *type, time);
                                if(type->flags & messages::ABORT)
                                    state_update(*conn, *type, time);
                            }
                            if(mode() == HALF_DUPLEX &&
                                    prev == connection_type::HALF_OPEN &&
                                    conn->state == connection_type::OPEN &&
                                    seekpos == HDRLEN && pos > seekpos
                            ){
                                messages::msgheader stop = {
                                    *eid, {1, sizeof(stop)},
                                    {0,0},{messages::STOP, 0}
                                };
                                for(auto c=connections().begin();
                                    (c < connections().end() &&
                                        c->state < connection_type::HALF_CLOSED);
                                    ++c
                                ){
                                    // This is a very awkward way to do this, but I have implemented it like this to keep
                                    // open the option of implementing UDP transport. With unreliable transports, it is
                                    // necessary to retry control messages until after the remote end sends back an ACK.
                                    if(auto sp = c->south.lock(); sp && c->uuid==*eid && sp != ssp){
                                        sp->write(reinterpret_cast<const char*>(&stop), sizeof(stop));
                                        triggers().set(sp->native_handle(), POLLOUT);
                                        for(int i=0; i<2; ++i)
                                            state_update(*c, stop.type, time);
                                    }
                                }
                            }
                            break;
                        } else conn = --connections().erase(conn);
                    }
                }
                if(!rem)
                    buf.setstate(std::ios_base::eofbit);
            }
            if(eof)
                return -1;
            return 0;
        }
        std::streamsize connector::_north_connect(const north_type& interface, const north_type::stream_ptr& nsp, marshaller_type::north_format& buf){
            constexpr std::size_t SHRINK_THRESHOLD = 4096;
            const auto n = connection_type::clock_type::now();
            auto eid = messages::make_uuid_v7();
            if(eid == messages::uuid{})
                return -1;
            connections_type connect;
            for(auto& sbd: south()){
                auto&[sockfd, sptr] = sbd.streams().empty() ? sbd.make() : sbd.streams().back();
                sbd.register_connect(
                    sptr,
                    [&triggers=triggers()](
                        auto& hnd,
                        const auto *addr,
                        auto addrlen,
                        const std::string& protocol
                    ){
                        auto&[sfd, ssp] = hnd;
                        if(!sfd){
                            if(protocol == "TCP" || protocol == "UNIX")
                                sfd = socket(addr->sa_family, SOCK_STREAM, 0);
                            else throw std::invalid_argument("Unsupported transport protocol.");
                            sfd = set_flags(sfd);
                            ssp->native_handle() = sfd;
                            ssp->connectto(addr, addrlen);
                        }
                        triggers.set(sfd, (POLLIN | POLLOUT));
                    }
                );
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
        void connector::_north_err_handler(north_type& interface, const north_type::handle_type& stream, event_mask& revents){
            messages::msgheader stop = {
                {}, {1, static_cast<std::uint16_t>(sizeof(stop))},
                {0,0},{messages::STOP, 0}
            };
            const auto time = connection_type::clock_type::now();
            const auto&[nfd, nsp] = stream;
            for(auto conn = connections().begin(); conn < connections().end(); ++conn){
                if(auto n = conn->north.lock(); n && n == nsp){
                    if(auto s = conn->south.lock()){
                        triggers().set(s->native_handle(), POLLOUT);
                        if(conn->state < connection_type::CLOSED &&
                                (n->fail() || !n->eof())
                        ){
                            stop.eid = conn->uuid;
                            if(n->fail()){
                                stop.type.flags = messages::ABORT;
                                state_update(*conn, stop.type, time);
                            }
                            s->write(reinterpret_cast<const char*>(&stop), sizeof(stop));
                        }
                        state_update(*conn, stop.type, time);
                        if(conn->state == connection_type::CLOSED)
                            conn = --connections().erase(conn);
                    } else conn = --connections().erase(conn);
                }
            }
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
            if(drain())
                return -1;
            int sockfd = 0, listenfd = std::get<north_type::native_handle_type>(stream);
            while((sockfd = _accept(listenfd, nullptr, nullptr)) >= 0){
                interface.make(sockfd, true);
                triggers().set(sockfd, POLLIN);
            }
            revents &= ~(POLLIN | POLLHUP);
            return 0;
        }
        void connector::_north_state_handler(north_type& interface, const north_type::handle_type& stream, event_mask& revents){
            const auto&[nfd, nsp] = stream;
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
                            triggers().set(s->native_handle(), POLLOUT);
                            if(s && conn->state == connection_type::HALF_CLOSED){
                                stop.eid = conn->uuid;
                                s->write(reinterpret_cast<const char*>(&stop), sizeof(stop));
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
                            return _north_err_handler(interface, stream, revents);
                        default: break;
                    }
                }
            }
        }
        int connector::_north_pollout_handler(const north_type::handle_type& stream, event_mask& revents){
            const auto&[nfd, nsp]=stream;
            if(revents & (POLLERR | POLLNVAL))
                nsp->setstate(std::ios_base::badbit);
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
            for(auto conn = connections().begin(); conn < connections().end(); ++conn){
                if(auto s = conn->south.lock(); s && s == ssp){
                    if(auto n = conn->north.lock()){
                        state_update(*conn, {messages::STOP, 0}, time);
                        triggers().set(n->native_handle(), POLLOUT);
                    } else conn = --connections().erase(conn);
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
        int connector::_south_pollout_handler(const south_type::handle_type& stream, event_mask& revents){
            const auto&[sfd, ssp] = stream;
            if(revents & (POLLERR | POLLNVAL))
                ssp->setstate(std::ios_base::badbit);
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