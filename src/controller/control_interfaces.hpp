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
#include "../interfaces.hpp"
#pragma once
#ifndef CLOUDBUS_CONTROLLER_INTERFACES
#define CLOUDBUS_CONTROLLER_INTERFACES
namespace cloudbus{
    namespace controller {
        class cs_north : public ss_interface {
            public:
                using Base = ss_interface;
                using traits_type = Base::traits_type;
                using stream_ptr = Base::stream_ptr;
                using native_handle_type = Base::native_handle_type;
                using stream_type = Base::stream_type;
                using streams_type = Base::stream_type;
                using storage_type = Base::storage_type;
                using size_type = Base::size_type;
                using address_type = Base::address_type;

                cs_north();
                cs_north(const address_type addr, size_type addr_len);
                cs_north(cs_north&& other);

                cs_north& operator=(cs_north&& other);              

                ~cs_north() = default;  

                cs_north(const cs_north& other) = delete;
                cs_north& operator=(const cs_north& other) = delete;                        
        };
        class cs_south : public cs_interface{
            public:
                using Base = cs_interface;
                using traits_type = Base::traits_type;
                using stream_ptr = Base::stream_ptr;
                using native_handle_type = Base::native_handle_type;
                using stream_type = Base::stream_type;
                using streams_type = Base::stream_type;
                using storage_type = Base::storage_type;
                using size_type = Base::size_type;
                using address_type = Base::address_type;

                cs_south();
                cs_south(const address_type addr, size_type addr_len);
                cs_south(cs_south&& other);

                cs_south& operator=(cs_south&& other);  

                ~cs_south() = default;

                cs_south(const cs_south& other) = delete;
                cs_south& operator=(const cs_south& other) = delete;                    
        };
    }
}
#endif