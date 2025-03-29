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
#include "controller_marshaller.hpp"
#include "../../connectors.hpp"
#pragma once
#ifndef CLOUDBUS_CONTROLLER_CONNECTOR
#define CLOUDBUS_CONTROLLER_CONNECTOR
namespace cloudbus {
    namespace controller {
        class connector: public basic_connector<controller::marshaller, handler_type>
        {
            public:
                using Base = basic_connector<controller::marshaller, handler_type>;
                connector(
                    trigger_type& triggers,
                    const config::configuration::section& section
                ): Base(triggers, section){}
                ~connector() = default;

                connector() = delete;
                connector(const connector& other) = delete;
                connector(connector&& other) = delete;
                connector& operator=(const connector& other) = delete;
                connector& operator=(connector&& other) = delete;

            protected:
                virtual size_type _handle(events_type& events) override;
                virtual int _route(marshaller_type::north_format& buf, const north_type& interface, const north_type::handle_type& stream, event_mask& revents) override;
                virtual int _route(marshaller_type::south_format& buf, const south_type& interface, const south_type::handle_type& stream, event_mask& revents) override;
                virtual std::streamsize _north_connect(const north_type& interface, const north_type::stream_ptr& nsp, marshaller_type::north_format& buf) override;

            private:
                void _north_err_handler(north_type& interface, const north_type::handle_type& stream, event_mask& revents);
                int _north_pollin_handler(const north_type& interface, const north_type::handle_type& stream, event_mask& revents);
                int _north_accept_handler(north_type& interface, const north_type::handle_type& stream, event_mask& revents);
                void _north_state_handler(north_type& interface, const north_type::handle_type& stream, event_mask& revents);
                int _north_pollout_handler(const north_type::handle_type& stream, event_mask& revents);
                size_type _handle(north_type& interface, const north_type::handle_type& stream, event_mask& revents);

                void _south_err_handler(south_type& interface, const south_type::handle_type& stream, event_mask& revents);
                std::streamsize _south_write(const north_type::stream_ptr& n, marshaller_type::south_format& buf);
                int _south_pollin_handler(const south_type& interface, const south_type::handle_type& stream, event_mask& revents);
                int _south_state_handler(const south_type::handle_type& stream);
                int _south_pollout_handler(const south_type::handle_type& stream, event_mask& revents);
                size_type _handle(south_type& interface, const south_type::handle_type& stream, event_mask& revents);
        };
    }
}
#endif