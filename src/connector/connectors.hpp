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
#include "connector_timerqueue.hpp"
#include "../config.hpp"
#include "../messages.hpp"
#include "../dns.hpp"
#pragma once
#ifndef CLOUDBUS_CONNECTOR
#define CLOUDBUS_CONNECTOR
namespace cloudbus {
    template<class WeakPtr>
    struct connection {
        using uuid_type = messages::uuid;
        using socket_type = WeakPtr;
        using clock_type = std::chrono::steady_clock;
        using time_point = clock_type::time_point;
        using times_type = std::array<time_point, 4>;
        using times_ptr = std::unique_ptr<times_type>;
        static connection make(
            const uuid_type& uuid,
            const socket_type& north,
            const socket_type& south,
            short state,
            const time_point& t = clock_type::now()
        ){
            times_type times{};
            for(short i=0; i <= state; ++i)
                times[i] = t;
            return connection{
                uuid,
                north,
                south,
                std::make_unique<times_type>(std::move(times)),
                state
            };
        }
        enum states {HALF_OPEN, OPEN, HALF_CLOSED, CLOSED};
        uuid_type uuid;
        socket_type north, south;
        times_ptr timestamps;
        short state;      
    };

    template<class MarshallerT>
    struct connector_traits {
        using marshaller_type = MarshallerT;
        using north_type = typename marshaller_type::north_type;
        using south_type = typename marshaller_type::south_type;
    };   

    class connector_base {
        public:
            using interface_type = interface_base;
            using interfaces = std::vector<interface_type>;
            using stream_ptr = std::weak_ptr<interface_base::stream_type>;

            using connection_type = connection<stream_ptr>;
            using clock_type = connection_type::clock_type;
            using connections_type = std::vector<connection_type>;

            enum modes {HALF_DUPLEX, FULL_DUPLEX};

            explicit connector_base(const config::section& section, int mode=HALF_DUPLEX);

            interface_base::native_handle_type make_north(const config::address_type& address);
            int make_south(const config::address_type& address);

            interfaces& north() { return _north; }
            interfaces& south() { return _south; }
            connections_type& connections() { return _connections; }
            TimerQueue& timeouts() { return _timeouts; }
            int& mode() { return _mode; }
            int& drain() { return _drain; }

            virtual ~connector_base();

            connector_base() = delete;
            connector_base(const connector_base& other) = delete;
            connector_base(connector_base&& other) = delete;
            connector_base& operator=(const connector_base& other) = delete;
            connector_base& operator=(connector_base&& other) = delete;

        private:
            interfaces _north, _south;
            connections_type _connections;
            TimerQueue _timeouts;
            int _mode, _drain;
    };

    template<class HandlerT>
    class connector_handler : public HandlerT, public connector_base
    {
        public:
            using Base = connector_base;
            using trigger_type = typename HandlerT::trigger_type;
            using size_type = typename HandlerT::size_type;
            using events_type = typename HandlerT::events_type;
            using resolver_type = dns::basic_resolver<HandlerT>;

            connector_handler(trigger_type& triggers, const config::section& section):
                HandlerT(triggers), Base(section), _resolver{triggers}
            {
                const auto& hnd = north().front().streams().front();
                auto sockfd = std::get<interface_base::native_handle_type>(hnd);
                triggers.set(sockfd, POLLIN);
            }

            resolver_type& resolver() { return _resolver; }

            virtual ~connector_handler() = default;

            connector_handler() = delete;
            connector_handler(const connector_handler& other) = delete;
            connector_handler(connector_handler&& other) = delete;
            connector_handler& operator=(const connector_handler& other) = delete;
            connector_handler& operator=(connector_handler&& other) = delete;

        protected:
            virtual size_type _handle(events_type& events) override {
                auto handled = _resolver.handle(events);
                Base::timeouts().processEvents();
                return handled;
            }

        private:
            resolver_type _resolver;
    };

    template<class MarshallerT>
    class connector_marshaller : public connector_traits<MarshallerT>
    {
        public:
            using Base = connector_traits<MarshallerT>;
            using marshaller_type = typename Base::marshaller_type;

            connector_marshaller() = default;
            marshaller_type& marshaller() { return _marshaller; }
            virtual ~connector_marshaller() = default;

            connector_marshaller(const connector_marshaller& other) = delete;
            connector_marshaller(connector_marshaller&& other) = delete;
            connector_marshaller& operator=(const connector_marshaller& other) = delete;
            connector_marshaller& operator=(connector_marshaller&& other) = delete;

        private:
            marshaller_type _marshaller;
    };

    template<class MarshallerT, class HandlerT>
    class basic_connector: 
            public connector_handler<HandlerT>,
            public connector_marshaller<MarshallerT>
    {
        public:
            using HandlerBase = connector_handler<HandlerT>;
            using trigger_type = typename HandlerBase::trigger_type;
            using event_mask = typename HandlerBase::event_mask;

            using MarshallerBase = connector_marshaller<MarshallerT>;
            using marshaller_type = typename MarshallerBase::marshaller_type;
            using north_type = typename MarshallerBase::north_type;
            using south_type = typename MarshallerBase::south_type;

            basic_connector(
                trigger_type& triggers,
                const config::section& section
            ):
                HandlerBase(triggers, section),
                MarshallerBase()
            {}
            int route(
                typename marshaller_type::north_format& buf,
                north_type& interface,
                const typename north_type::handle_type& stream,
                event_mask& revents
            ){
                return _route(buf, interface, stream, revents);
            }
            int route(
                typename marshaller_type::south_format& buf,
                south_type& interface,
                const typename south_type::handle_type& stream,
                event_mask& revents
            ){
                return _route(buf, interface, stream, revents);
            }
            std::streamsize north_connect(
                north_type& interface,
                const typename north_type::stream_ptr& nsp,
                typename marshaller_type::north_format& buf
            ){
                return _north_connect(interface, nsp, buf);
            }

            virtual ~basic_connector() = default;

            basic_connector() = delete;
            basic_connector(const basic_connector& other) = delete;
            basic_connector(basic_connector&& other) = delete;
            basic_connector& operator=(const basic_connector& other) = delete;
            basic_connector& operator=(basic_connector&& other) = delete;

        protected:
            virtual int _route(
                typename marshaller_type::north_format& buf,
                north_type& interface,
                const typename north_type::handle_type& stream,
                event_mask& revents
            ){ return -1; }

            virtual int _route(
                typename marshaller_type::south_format& buf,
                south_type& interface,
                const typename south_type::handle_type& stream,
                event_mask& revents
            ){ return -1; }

            virtual std::streamsize _north_connect(
                north_type& interface,
                const typename north_type::stream_ptr& nsp,
                typename marshaller_type::north_format& buf
            ){ return -1; }
    };
}
#endif
