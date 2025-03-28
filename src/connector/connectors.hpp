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
#include "../config.hpp"
#include "../interfaces.hpp"
#include "../messages.hpp"
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
        enum states {HALF_OPEN, OPEN, HALF_CLOSED, CLOSED};
        times_type timestamps;
        uuid_type uuid;
        socket_type north, south;
        int state;
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
            using connections_type = std::vector<connection_type>;
            enum modes {HALF_DUPLEX, FULL_DUPLEX};

            explicit connector_base(const config::configuration::section& section, int mode=HALF_DUPLEX);

            interface_base::native_handle_type make_north(const config::address_type& address);
            int make_south(const config::address_type& address);

            interfaces& north() { return _north; }
            interfaces& south() { return _south; }
            connections_type& connections() { return _connections; }
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
            int _mode, _drain;
    };

    template<class HandlerT>
    class connector_handler : public HandlerT, public connector_base
    {
        public:
            using HandlerBase = HandlerT;
            using ConnectorBase = connector_base;
            using trigger_type = typename HandlerBase::trigger_type;

            connector_handler(trigger_type& triggers, const config::configuration::section& section):
                HandlerBase(triggers), ConnectorBase(section)
            {
                const auto& hnd = north().front().streams().front();
                auto sockfd = std::get<interface_base::native_handle_type>(hnd);
                triggers.set(sockfd, POLLIN);
            }

            virtual ~connector_handler() = default;

            connector_handler() = delete;
            connector_handler(const connector_handler& other) = delete;
            connector_handler(connector_handler&& other) = delete;
            connector_handler& operator=(const connector_handler& other) = delete;
            connector_handler& operator=(connector_handler&& other) = delete;
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
    class basic_connector: public connector_handler<HandlerT>, public connector_marshaller<MarshallerT>
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
                const config::configuration::section& section
            ):
                HandlerBase(triggers, section){}

            int route(
                typename marshaller_type::north_format& buf,
                const north_type& interface,
                const typename north_type::handle_type& stream,
                event_mask& revents
            ){
                return _route(buf, interface, stream, revents);
            }
            int route(
                typename marshaller_type::south_format& buf,
                const south_type& interface,
                const typename south_type::handle_type& stream,
                event_mask& revents
            ){
                return _route(buf, interface, stream, revents);
            }
            std::streamsize north_connect(
                const north_type& interface,
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
                const north_type& interface,
                const typename north_type::handle_type& stream,
                event_mask& revents
            ){ return -1; }

            virtual int _route(
                typename marshaller_type::south_format& buf,
                const south_type& interface,
                const typename south_type::handle_type& stream,
                event_mask& revents
            ){ return -1; }

            virtual std::streamsize _north_connect(
                const north_type& interface,
                const typename north_type::stream_ptr& nsp,
                typename marshaller_type::north_format& buf
            ){ return -1; }
    };
}
#endif