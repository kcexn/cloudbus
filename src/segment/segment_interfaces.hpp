/* 
*   Copyright 2025 Kevin ExtonGNU Affero General Public License
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
#ifndef CLOUDBUS_SEGMENT_INTERFACES
#define CLOUDBUS_SEGMENT_INTERFACES
namespace cloudbus{
    namespace segment {
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
        class service_interface : public ss_interface
        {
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

                service_interface();
                service_interface(const address_type addr, size_type addrlen);
                service_interface(ss_interface&& other);

                service_interface& operator=(service_interface&& other);

                service_interface(const service_interface& other) = delete;
                service_interface& operator=(const service_interface& other) = delete;  
        };
    }
}
#endif