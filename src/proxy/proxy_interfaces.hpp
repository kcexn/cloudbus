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
        class cs_north : public cs_interface {
            public:
                using Base = cs_interface;

                cs_north():
                    cs_north(std::string(), nullptr, 0){}
                cs_north(const std::string& uri):
                    cs_north(uri, nullptr, 0){}
                cs_north(const struct sockaddr *addr, socklen_t addrlen):
                    cs_north(std::string(), addr, addrlen){}
                cs_north(const addresses_type& addresses):
                    cs_north(std::string(), addresses){}
                cs_north(addresses_type&& addresses):
                    cs_north(std::string(), std::move(addresses)){}
                explicit cs_north(const std::string& uri, const struct sockaddr *addr, socklen_t addrlen):
                    Base(uri, addr, addrlen){}
                explicit cs_north(const std::string& uri, const addresses_type& addresses):
                    Base(uri, addresses){}
                explicit cs_north(const std::string& uri, addresses_type&& addresses):
                    Base(uri, std::move(addresses)){}
                explicit cs_north(cs_north&& other):
                    Base(std::move(other)){}
                cs_north& operator=(cs_north&& other){
                    Base::operator=(std::move(other));
                    return *this;
                }

                ~cs_north() = default;

                cs_north(const cs_north& other) = delete;
                cs_north& operator=(const cs_north& other) = delete;                        
        };
        class cs_south : public cs_interface {
            public:
                using Base = cs_interface;

                cs_south():
                    cs_south(std::string(), nullptr, 0){}
                cs_south(const std::string& uri):
                    cs_south(uri, nullptr, 0){}
                cs_south(const struct sockaddr *addr, socklen_t addrlen):
                    cs_south(std::string(), addr, addrlen){}
                cs_south(const addresses_type& addresses):
                    cs_south(std::string(), addresses){}
                cs_south(addresses_type&& addresses):
                    cs_south(std::string(), std::move(addresses)){}
                explicit cs_south(const std::string& uri, const struct sockaddr *addr, socklen_t addrlen):
                    Base(uri, addr, addrlen){}
                explicit cs_south(const std::string& uri, const addresses_type& addresses):
                    Base(uri, addresses){}
                explicit cs_south(const std::string& uri, addresses_type&& addresses):
                    Base(uri, std::move(addresses)){}
                explicit cs_south(cs_south&& other):
                    Base(std::move(other)){}
                cs_south& operator=(cs_south&& other){
                    Base::operator=(std::move(other));
                    return *this;
                }

                ~cs_south() = default;

                cs_south(const cs_south& other) = delete;
                cs_south& operator=(const cs_south& other) = delete;                    
        };
    }
}
#endif