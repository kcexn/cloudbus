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
#include "../io.hpp"
#include "../connectors.hpp"
#include "../registry.hpp"
#include "segment_marshallers.hpp"
#pragma once
#ifndef CLOUDBUS_SEGMENT_CONNECTOR
#define CLOUDBUS_SEGMENT_CONNECTOR
namespace cloudbus {
    namespace segment {
        using handler_type = ::io::basic_handler<::io::trigger>;
        class segment_connector: public basic_connector<segment_marshaller, handler_type>
        {
            public:
                using Base = basic_connector<segment_marshaller, handler_type>;
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

                segment_connector(trigger_type& triggers);
                norths_type::iterator make(norths_type& n, const registry::address_type& address);
                souths_type::iterator make(souths_type& n, const registry::address_type& address);

                int route(marshaller_type::north_format& buf, const shared_north& interface, const north_type::handle_ptr& stream, event_mask& revents){ 
                    return _route(buf, interface, stream, revents);
                }
                int route(marshaller_type::south_format& buf, const shared_south& interface, const south_type::handle_ptr& stream, event_mask& revents){ 
                    return _route(buf, interface, stream, revents);
                }
                std::streamsize north_connect(const shared_north& interface, const north_type::stream_ptr& nsp, marshaller_type::north_format& buf){
                    return _north_connect(interface, nsp, buf);
                }                

                ~segment_connector() = default;

                segment_connector() = delete;
                segment_connector(const segment_connector& other) = delete;
                segment_connector(segment_connector&& other) = delete;
                segment_connector& operator=(const segment_connector& other) = delete;
                segment_connector& operator=(segment_connector&& other) = delete;               

            protected:
                virtual size_type _handle(events_type& events) override;
                virtual int _route(marshaller_type::north_format& buf, const shared_north& interface, const north_type::handle_ptr& stream, event_mask& revents);
                virtual int _route(marshaller_type::south_format& buf, const shared_south& interface, const south_type::handle_ptr& stream, event_mask& revents);
                virtual std::streamsize _north_connect(const shared_north& interface, const north_type::stream_ptr& nsp, marshaller_type::north_format& buf);

            private:
                void _north_err_handler(const shared_north& interface, const north_type::handle_ptr& stream, event_mask& revents);
                std::streamsize _north_write(const south_type::stream_ptr& s, marshaller_type::north_format& buf);
                int _north_pollin_handler(const shared_north& interface, const north_type::handle_ptr& stream, event_mask& revents);
                int _north_accept_handler(const shared_north& interface, const north_type::handle_ptr& stream, event_mask& revents);
                int _north_pollout_handler(const north_type::handle_ptr& stream, event_mask& revents);
                size_type _handle(const shared_north& interface, const north_type::handle_ptr& stream, event_mask& revents);

                void _south_err_handler(const shared_south& interface, const south_type::handle_ptr& stream, event_mask& revents);
                std::streamsize _south_write(const north_type::stream_ptr& n, const connection_type& conn, marshaller_type::south_format& buf);
                int _south_pollin_handler(const shared_south& interface, const south_type::handle_ptr& stream, event_mask& revents);
                void _south_state_handler(const shared_south& interface, const south_type::handle_ptr& stream, event_mask& revents);
                int _south_pollout_handler(const south_type::handle_ptr& stream, event_mask& revents);
                size_type _handle(const shared_south& interface, const south_type::handle_ptr& stream, event_mask& revents);
        }; 
    }
}
#endif