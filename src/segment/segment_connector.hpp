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
                norths_type::iterator make(norths_type& n, const north_type::address_type addr, north_type::size_type addrlen);
                souths_type::iterator make(souths_type& s, const south_type::address_type addr, south_type::size_type addrlen);                 

                ~segment_connector() = default;

                segment_connector() = delete;
                segment_connector(const segment_connector& other) = delete;
                segment_connector(segment_connector&& other) = delete;
                segment_connector& operator=(const segment_connector& other) = delete;
                segment_connector& operator=(segment_connector&& other) = delete;               

            protected:
                virtual int _handle(events_type& events) override;

            private:
                void _north_err_handler(shared_north& interface, const north_type::stream_type& stream, event_mask& revents);
                std::streamsize _north_write(south_type::stream_ptr& s, marshaller_type::north_format& buf);
                std::streamsize _north_connect_handler(const shared_north& interface, north_type::stream_ptr& nsp, marshaller_type::north_format& buf);
                int _north_pollin_handler(const shared_north& interface, north_type::stream_type& stream, event_mask& revents);
                int _north_accept_handler(shared_north& interface, const north_type::stream_type& stream, event_mask& revents);
                int _north_pollout_handler(north_type::stream_type& stream, event_mask& revents);
                int _handle(shared_north& interface, north_type::stream_type& stream, event_mask& revents);

                void _south_err_handler(shared_south& interface, const south_type::stream_type& stream, event_mask& revents);
                std::streamsize _south_write(north_type::stream_ptr& n, const connection_type& conn, marshaller_type::south_format& buf);
                int _south_pollin_handler(shared_south& interface, south_type::stream_type& stream, event_mask& revents);
                void _south_state_handler(shared_south& interface, const south_type::stream_type& stream, event_mask& revents);
                int _south_pollout_handler(south_type::stream_type& stream, event_mask& revents);
                int _handle(shared_south& interface, south_type::stream_type& stream, event_mask& revents);
        }; 
    }
}
#endif