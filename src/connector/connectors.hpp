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
#include "../messages.hpp"
#include <memory>
#include <tuple>
#include <array>
#include <chrono>
#pragma once
#ifndef CLOUDBUS_CONNECTOR
#define CLOUDBUS_CONNECTOR
namespace cloudbus {
    template<class NorthWeakPtr, class SouthWeakPtr>
    struct connection {
        using uuid_type = messages::uuid;
        using north_ptr = NorthWeakPtr;
        using south_ptr = SouthWeakPtr;
        using clock_type = std::chrono::steady_clock;
        using time_point = clock_type::time_point;
        using times_type = std::array<time_point, 4>;
        enum states {HALF_OPEN, OPEN, HALF_CLOSED, CLOSED};
        uuid_type uuid;
        north_ptr north;
        south_ptr south;
        int state;
        times_type timestamps;
    };
    template<class MarshallerT, class HandlerT>
    class basic_connector: public HandlerT
    {
        public:
            using Base = HandlerT;
            using trigger_type = typename Base::trigger_type;
            using event_type = typename Base::event_type;
            using events_type = typename Base::events_type;
            using event_mask = typename Base::event_mask;
            using size_type = typename Base::size_type;    

            using marshaller_type = MarshallerT;
            using north_type = typename marshaller_type::north_type;
            using shared_north = std::shared_ptr<north_type>;
            static shared_north make_north(const typename north_type::address_type addr, typename north_type::size_type addrlen){
                return std::make_shared<north_type>(addr, addrlen);
            }            
            using norths_type = std::vector<shared_north>;
            using north_ptr = typename marshaller_type::north_ptr;

            using south_type = typename marshaller_type::south_type;
            using shared_south = std::shared_ptr<south_type>;
            static shared_south make_south(const typename south_type::address_type addr, typename south_type::size_type addrlen){
                return std::make_shared<south_type>(addr, addrlen);
            }
            using souths_type = std::vector<shared_south>;
            using south_ptr = typename marshaller_type::south_ptr;

            using connection_type = connection<north_ptr, south_ptr>;
            using connections_type = std::vector<connection_type>;
            enum modes {HALF_DUPLEX, FULL_DUPLEX};

            basic_connector(trigger_type& triggers): _triggers{triggers}{}

            trigger_type& triggers() { return _triggers; }
            norths_type& north() { return _north; }
            souths_type& south() { return _south; }
            connections_type& connections() { return _connections; }
            marshaller_type& marshaller() { return _marshaller; }
            int& mode() { return _mode; }
            
            virtual ~basic_connector() = default;

            basic_connector() = delete;
            basic_connector(const basic_connector& other) = delete;
            basic_connector(basic_connector&& other) = delete;
            basic_connector& operator=(const basic_connector& other) = delete;
            basic_connector& operator=(basic_connector&& other) = delete;            
        private:
            trigger_type& _triggers;
            norths_type _north;
            souths_type _south;
            connections_type _connections;
            marshaller_type _marshaller;
            int _mode;
    };
}
#endif