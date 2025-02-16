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
#include "../interfaces.hpp"
#pragma once
#ifndef CLOUDBUS_PROXY_INTERFACES
#define CLOUDBUS_PROXY_INTERFACES
namespace cloudbus{
    namespace proxy {
        class cbus_interface : public cs_interface
        {
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

                cbus_interface();
                cbus_interface(const address_type addr, size_type addrlen);
                cbus_interface(cbus_interface&& other);

                cbus_interface& operator=(cbus_interface&& other);

                cbus_interface(const cbus_interface& other) = delete;
                cbus_interface& operator=(const cbus_interface& other) = delete;
        };    
        class cs_north : public cbus_interface
        {
            public:
                using Base = cbus_interface;
                using traits_type = Base::traits_type;
                using stream_ptr = Base::stream_ptr;
                using native_handle_type = Base::native_handle_type;
                using stream_type = Base::stream_type;
                using streams_type = Base::stream_type;
                using storage_type = Base::storage_type;
                using size_type = Base::size_type;
                using address_type = Base::address_type;  

                cs_north(): Base(){}
                cs_north(const address_type addr, size_type addrlen): Base(addr, addrlen){}
                cs_north(cs_north&& other): Base(std::move(other)){}

                cs_north& operator=(cs_north&& other){
                    Base::operator=(std::move(other));
                    return *this;
                }

                cs_north(const cs_north& other) = delete;
                cs_north& operator=(const cs_north& other) = delete;
        };
        class cs_south : public cbus_interface
        {
            public:
                using Base = cbus_interface;
                using traits_type = Base::traits_type;
                using stream_ptr = Base::stream_ptr;
                using native_handle_type = Base::native_handle_type;
                using stream_type = Base::stream_type;
                using streams_type = Base::stream_type;
                using storage_type = Base::storage_type;
                using size_type = Base::size_type;
                using address_type = Base::address_type;  

                cs_south(): Base(){}
                cs_south(const address_type addr, size_type addrlen): Base(addr, addrlen){}
                cs_south(cs_south&& other): Base(std::move(other)){}

                cs_south& operator=(cs_south&& other){
                    Base::operator=(std::move(other));
                    return *this;
                }

                cs_south(const cs_south& other) = delete;
                cs_south& operator=(const cs_south& other) = delete;
        };
    }
}
#endif