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
#include "../connectors.hpp"
#include "../io.hpp"
#include "control_marshallers.hpp"

#pragma once
#ifndef CLOUDBUS_CONTROLLER_CONNECTOR
#define CLOUDBUS_CONTROLLER_CONNECTOR
namespace cloudbus {
    namespace controller {
        using handler_type = ::io::basic_handler<::io::trigger>;
        class control_connector: public basic_connector<control_marshaller, handler_type>
        {
            public:
                using Base = basic_connector<control_marshaller, handler_type>;
                using trigger_type = Base::trigger_type;
                using event_type = Base::event_type;
                using events_type = Base::events_type;
                using event_mask = Base::event_mask;
                using size_type = Base::size_type;

                using marshaller_type = Base::marshaller_type;
                using north_type = Base::north_type;
                using shared_north = Base::shared_north;
                using norths_type = Base::norths_type;
                using north_ptr = Base::north_ptr;

                using south_type = Base::south_type;
                using shared_south = Base::shared_south;
                using souths_type = Base::souths_type;
                using south_ptr = Base::south_ptr;

                using connection_type = Base::connection_type;
                using connections_type = Base::connections_type;

                control_connector(trigger_type& triggers);
                norths_type::iterator make(norths_type& n, const struct sockaddr *addr, socklen_t addrlen);
                souths_type::iterator make(souths_type& s, const struct sockaddr *addr, socklen_t addrlen);

                int route(marshaller_type::north_format& buf, const shared_north& interface, const north_type::handle_ptr& stream, event_mask& revents){ 
                    return _route(buf, interface, stream, revents);
                }
                int route(marshaller_type::south_format& buf, const shared_south& interface, const south_type::handle_ptr& stream, event_mask& revents){ 
                    return _route(buf, interface, stream, revents);
                }
                std::streamsize north_connect(const shared_north& interface, const north_type::stream_ptr& nsp, marshaller_type::north_format& buf){
                    return _north_connect(interface, nsp, buf);
                }

                ~control_connector() = default;

                control_connector() = delete;
                control_connector(const control_connector& other) = delete;
                control_connector(control_connector&& other) = delete;
                control_connector& operator=(const control_connector& other) = delete;
                control_connector& operator=(control_connector&& other) = delete;               

            protected:
                virtual size_type _handle(events_type& events) override;
                virtual int _route(marshaller_type::north_format& buf, const shared_north& interface, const north_type::handle_ptr& stream, event_mask& revents);
                virtual int _route(marshaller_type::south_format& buf, const shared_south& interface, const south_type::handle_ptr& stream, event_mask& revents);
                virtual std::streamsize _north_connect(const shared_north& interface, const north_type::stream_ptr& nsp, marshaller_type::north_format& buf);

            private:
                void _north_err_handler(const shared_north& interface, const north_type::handle_ptr& stream, event_mask& revents);
                int _north_pollin_handler(const shared_north& interface, const north_type::handle_ptr& stream, event_mask& revents);
                int _north_accept_handler(const shared_north& interface, const north_type::handle_ptr& stream, event_mask& revents);
                void _north_state_handler(const shared_north& interface, const north_type::handle_ptr& stream, event_mask& revents);
                int _north_pollout_handler(const north_type::handle_ptr& stream, event_mask& revents);
                size_type _handle(const shared_north& interface, const north_type::handle_ptr& stream, event_mask& revents);

                void _south_err_handler(const shared_south& interface, const south_type::handle_ptr& stream, event_mask& revents);
                std::streamsize _south_write(const north_type::stream_ptr& n, marshaller_type::south_format& buf);
                int _south_pollin_handler(const shared_south& interface, const south_type::handle_ptr& stream, event_mask& revents);
                int _south_state_handler(const south_type::handle_ptr& stream);
                int _south_pollout_handler(const south_type::handle_ptr& stream, event_mask& revents);
                size_type _handle(const shared_south& interface, const south_type::handle_ptr& stream, event_mask& revents);
        };
    }
}
#endif