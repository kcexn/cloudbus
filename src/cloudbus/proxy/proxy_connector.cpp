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
#include "proxy_connector.hpp"
#include <sys/un.h>
#include <fcntl.h>
namespace cloudbus{
    namespace proxy {
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
        static std::streamsize stream_write(std::ostream& os, std::istream& is, std::streamsize len){
            constexpr std::streamsize BUFLEN=256;
            std::array<char, BUFLEN> buf;
            std::streamsize pos=MAX_BUFSIZE-len;
            if(os.fail()) return -1;
            if(os.tellp() >= pos && os.flush().bad())
                return -1;
            if(os.tellp() < pos){
                std::streamsize count=0, n=std::min(len-count, BUFLEN);
                while(auto gcount = is.readsome(buf.data(), n)){
                    if(os.write(buf.data(), gcount).bad())
                        return -1;
                    if( (count+=gcount) == len )
                        return count;
                    n=std::min(len-count, BUFLEN);
                }
                return count;
            } else return 0;
        }
        static connector::connections_type::iterator write_prepare(
            connector::connections_type& connections,
            const messages::uuid& uuid,
            const std::streamsize& len
        ){
            const std::streamsize pos = MAX_BUFSIZE-len;
            for(auto conn=connections.begin(); conn < connections.end(); ++conn){
                if(!messages::uuidcmp_node(&conn->uuid, &uuid)){
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
            const connector::events_type::iterator& ev,
            connector::events_type::iterator& put
        ){
            triggers.set(sockfd, POLLIN);
            auto it = std::find_if(events.begin(), events.end(), [&](auto& e){ return e.fd == sockfd; });
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
            for(auto ev=events.begin(); ev < events.end(); ++ev){
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
                                            if(!c.north.owner_before(nsp))
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
                                        const auto&[sockfd, ssp] = *stream;
                                        if(sockfd==ev->fd && ev->revents & (POLLOUT | POLLERR))
                                            for(auto& c: connections())
                                                if(!c.south.owner_before(ssp))
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
            auto&[nfd, nsp] = stream;
            const auto eof = nsp->eof();
            if(const auto *type = buf.type()){
                auto *eid = buf.eid();
                const auto pos=buf.tellp(), seekpos=buf.tellg();
                const auto rem = buf.len()->length - pos;
                std::vector<char> padding;
                if(write_prepare(connections(), *eid, pos-seekpos) != connections().end())
                    return clear_triggers(nfd, triggers(), revents, (POLLIN | POLLHUP));
                const auto time = connector::connection_type::clock_type::now();
                std::size_t connected = 0;
                for(auto c=connections().begin(); c < connections().end(); ++c){
                    if(c->south.expired()){
                        c = --connections().erase(c);
                    } else if(!messages::uuidcmp_node(&c->uuid, eid) &&
                        ++connected &&
                        c->state < connection_type::CLOSED
                    ){
                        if(auto s = c->south.lock()){
                            *eid = c->uuid;
                            stream_write(*s, buf.seekg(seekpos), pos-seekpos);
                            if(auto sockfd = s->native_handle(); sockfd >= 0)
                                triggers().set(sockfd, POLLOUT);
                            if(!rem) {
                                state_update(*c, *type, time);
                                if(type->op == messages::STOP &&
                                    (type->flags & messages::ABORT)
                                ){
                                    state_update(*c, *type, time);
                                }
                            } else if(eof) {
                                padding.resize(rem);
                                s->write(padding.data(), padding.size());
                            }
                        } else c = --connections().erase(c);
                    }
                }
                if(!eof && !connected && (type->op & messages::INIT)){
                    if(auto status = north_connect(interface, nsp, buf)){
                        if(status < 0)
                            return -1;
                    } else return clear_triggers(nfd, triggers(), revents, (POLLIN | POLLHUP));
                }
                if(!rem)
                    buf.setstate(buf.eofbit);
            }
            if(eof)
                return -1;
            return 0;
        }
        int connector::_route(marshaller_type::south_format& buf, const south_type& interface, const south_type::handle_type& stream, event_mask& revents){
            auto&[sfd, ssp] = stream;
            const auto eof = ssp->eof();
            if(const auto *type = buf.type()){
                const auto *eid = buf.eid();
                const auto pos=buf.tellp(), seekpos=buf.tellg();
                const auto rem = buf.len()->length - pos;
                std::vector<char> padding;
                for(auto conn = connections().begin(); conn < connections().end(); ++conn){
                    if(conn->north.expired()){
                        conn = --connections().erase(conn);
                    } else if(!messages::uuidcmp_node(&conn->uuid, eid) &&
                            !conn->south.owner_before(ssp)
                    ){
                        if(conn->state == connection_type::CLOSED){
                            if(type->op == messages::STOP)
                                conn = connections().erase(conn);
                            break;
                        } else if(auto n = conn->north.lock()){
                            if(mode()==FULL_DUPLEX && rem && !eof)
                                break;
                            const auto time = connection_type::clock_type::now();
                            if(auto len = pos-seekpos){
                                triggers().set(n->native_handle(), POLLOUT);
                                if(!stream_write(*n, buf.seekg(seekpos), len))
                                    return clear_triggers(sfd, triggers(), revents, (POLLIN | POLLHUP));
                                auto prev = conn->state;
                                if(!rem){
                                    state_update(*conn, *type, time);
                                    if(type->op == messages::STOP &&
                                            (type->flags & messages::ABORT)
                                    ){
                                        state_update(*conn, *type, time);
                                    }
                                } else if(eof){
                                    padding.resize(rem);
                                    n->write(padding.data(), padding.size());
                                    triggers().set(n->native_handle(), POLLOUT);
                                }
                                if(mode() == HALF_DUPLEX &&
                                    prev == connection_type::HALF_OPEN &&
                                    conn->state == connection_type::OPEN &&
                                    seekpos==0
                                ){
                                    const messages::msgheader stop = {
                                        *eid, {1, sizeof(stop)},
                                        {0,0},{messages::STOP, 0}
                                    };
                                    for(auto c=connections().begin(); c < connections().end() && c->state < connection_type::HALF_CLOSED; ++c){
                                        // This is a very awkward way to do this, but I have implemented it like this to keep
                                        // open the option of implementing UDP transport. With unreliable transports, it is
                                        // necessary to retry control messages until after the remote end sends back an ACK.
                                        if(c->uuid==*eid && c->south.owner_before(ssp)){
                                            if(auto sp = c->south.lock()){
                                                sp->write(reinterpret_cast<const char*>(&stop), sizeof(stop));
                                                triggers().set(sp->native_handle(), POLLOUT);
                                                state_update(*c, stop.type, time);
                                                state_update(*c, stop.type, time);
                                            }
                                        }
                                    }
                                }
                            }
                            break;
                        } else conn = --connections().erase(conn);
                    }
                }
                if(!rem)
                    buf.setstate(buf.eofbit);
            }
            if(eof)
                return -1;
            return 0;
        }
        std::streamsize connector::_north_connect(const north_type& interface, const north_type::stream_ptr& nsp, marshaller_type::north_format& buf){
            constexpr std::size_t SHRINK_THRESHOLD = 4096;
            const auto n = connection_type::clock_type::now();
            connections_type connect;
            for(auto& sbd: south()){
                auto&[sfd, ssp] = sbd.streams().empty() ? sbd.make() : sbd.streams().back();
                sbd.register_connect(
                    ssp,
                    [&triggers=triggers()](
                        auto& hnd,
                        const auto *addr,
                        auto addrlen,
                        const std::string& protocol
                    ){
                        auto&[sfd, ssp] = hnd;
                        if(sfd == ssp->BAD_SOCKET){
                            if(protocol == "TCP" || protocol == "UNIX")
                            {
                                if( (sfd = socket(addr->sa_family, SOCK_STREAM, 0)) < 0 )
                                {
                                    throw std::runtime_error("Unable to create new socket.");
                                }
                            }
                            else throw std::invalid_argument("Unsupported transport protocol.");
                            sfd = set_flags(sfd);
                            ssp->native_handle() = sfd;
                            ssp->connectto(addr, addrlen);
                        }
                        triggers.set(sfd, (POLLIN | POLLOUT));
                    }
                );
                if(sbd.addresses().empty())
                    resolver().resolve(sbd);
                if(Base::mode() == Base::FULL_DUPLEX){
                    if((buf.eid()->clock_seq_reserved & messages::CLOCK_SEQ_MAX) == messages::CLOCK_SEQ_MAX)
                        buf.eid()->clock_seq_reserved &= ~messages::CLOCK_SEQ_MAX;
                    else ++buf.eid()->clock_seq_reserved;
                }
                connect.push_back(
                    (buf.type()->op==messages::STOP)
                    ? connection_type{{n,n,n,{}}, *buf.eid(), nsp, ssp, connection_type::HALF_CLOSED}
                    : connection_type{{n,{},{},{}}, *buf.eid(), nsp, ssp, connection_type::HALF_OPEN}
                );
            }
            connections().insert(connections().cend(), connect.cbegin(), connect.cend());
            if(connections().capacity() > SHRINK_THRESHOLD)
                connections().shrink_to_fit();
            const std::streamsize len = buf.tellp() - buf.tellg();
            if(write_prepare(connect, *buf.eid(), len) == connect.end()){
                for(auto& c: connect)
                    if(auto s = c.south.lock()){
                        *buf.eid() = c.uuid;
                        stream_write(*s, buf.seekg(0), len);
                    }
                return len;
            }
            return 0;
        }
        void connector::_north_err_handler(north_type& interface, const north_type::handle_type& stream, event_mask& revents){
            auto&[nfd, nsp] = stream;
            const auto time = connection_type::clock_type::now();
            messages::msgheader stop{
                {}, {1, sizeof(stop)},
                {0,0}, {messages::STOP, 0}
            };
            for(auto& c: connections()){
                if(!c.north.owner_before(nsp)){
                    if(auto s = c.south.lock()){
                        stop.eid = c.uuid;
                        s->write(reinterpret_cast<const char*>(&stop), sizeof(stop));
                        triggers().set(s->native_handle(), POLLOUT);
                        state_update(c, stop.type, time);
                    }
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
        void connector::_north_state_handler(const north_type& interface, const north_type::handle_type& stream, event_mask& revents){
            const auto&[nfd, nsp] = stream;
            connections_type states;
            for(auto& c: connections()){
                if(!c.north.owner_before(nsp)){
                    auto it = std::find_if(states.begin(), states.end(), [&](auto& sc){ return !messages::uuidcmp_node(&sc.uuid, &c.uuid); });
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
            for(auto conn = connections().begin(); conn < connections().end(); ++conn){
                if(!conn->north.owner_before(nsp)){
                    auto it = std::find_if(states.cbegin(), states.cend(), [&](const auto& sc){ return !messages::uuidcmp_node(&sc.uuid, &conn->uuid); });
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
                            stop.eid = conn->uuid;
                            nsp->write(reinterpret_cast<const char*>(&stop), sizeof(stop));
                            triggers().set(nfd, POLLOUT);
                            return;
                        case connection_type::CLOSED:
                            if(s) triggers().set(s->native_handle(), POLLOUT);
                            conn = --connections().erase(conn);
                        default: break;
                    }
                }
            }
        }
        int connector::_north_pollout_handler(const north_type::handle_type& stream, event_mask& revents){
            const auto&[nfd, nsp] = stream;
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
                if(stream == interface.streams().front()){
                    if(_north_accept_handler(interface, stream, revents))
                        _north_err_handler(interface, stream, revents);
                } else if(_north_pollin_handler(interface, stream, revents))
                    _north_err_handler(interface, stream, revents);
            }
            return handled;
        }

        void connector::_south_err_handler(south_type& interface, const south_type::handle_type& stream, event_mask& revents){
            auto&[sfd, ssp] = stream;
            const auto time = connection_type::clock_type::now();
            messages::msgheader stop{
                {}, {1, sizeof(stop)},
                {0,0}, {messages::STOP, 0}
            };
            for(auto& c: connections()){
                if(!c.south.owner_before(ssp)){
                    if(auto n = c.north.lock()){
                        stop.eid = c.uuid;
                        n->write(reinterpret_cast<const char*>(&stop), sizeof(stop));
                        triggers().set(n->native_handle(), POLLOUT);
                        state_update(c, stop.type, time);
                    }
                }
            }
            revents = 0;
            triggers().clear(sfd);
            interface.erase(stream);
        }
        int connector::_south_pollin_handler(const south_type& interface, const south_type::handle_type& stream, event_mask& revents){
            auto it = marshaller().marshal(stream);
            if(std::get<south_type::stream_ptr>(stream)->gcount() == 0)
                revents &= ~(POLLIN | POLLHUP);
            return route(std::get<marshaller_type::south_format>(*it), interface, stream, revents);
        }
        int connector::_south_state_handler(const south_type::handle_type& stream){
            std::size_t count = 0;
            const auto& ssp = std::get<south_type::stream_ptr>(stream);
            for(auto conn = connections().begin(); conn < connections().end(); ++conn){
                if(!conn->south.owner_before(ssp)){
                    if(conn->state != connection_type::CLOSED)
                        ++count;
                    else conn = --connections().erase(conn);
                }
            }
            if(count == 0) return -1;
            return 0;
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
            if(_south_pollout_handler(stream, revents))
                _south_err_handler(interface, stream, revents);
            else if (_south_state_handler(stream))
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