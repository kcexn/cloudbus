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
                    cs_north(std::string(), addresses_type()){}
                cs_north(const std::string& uri):
                    cs_north(uri, addresses_type()){}
                cs_north(const std::string& uri, const std::string& protocol):
                    cs_north(uri, addresses_type(), protocol){}
                cs_north(const struct sockaddr *addr, socklen_t addrlen, const std::string& protocol, const duration_type& ttl=duration_type(-1)):
                    cs_north(std::string(), addr, addrlen, protocol, ttl){}              
                cs_north(const addresses_type& addresses, const std::string& protocol, const duration_type& ttl=duration_type(-1)):
                    cs_north(std::string(), addresses, protocol, ttl){}
                cs_north(addresses_type&& addresses, const std::string& protocol, const duration_type& ttl=duration_type(-1)):
                    cs_north(std::string(), std::move(addresses), protocol, ttl){}
                explicit cs_north(
                    const std::string& uri,
                    const struct sockaddr *addr,
                    socklen_t addrlen,
                    const std::string& protocol,
                    const duration_type& ttl=duration_type(-1)
                ): Base(uri, addr, addrlen, protocol, ttl){}
                explicit cs_north(
                    const std::string& uri,
                    const addresses_type& addresses,
                    const std::string& protocol=std::string(),
                    const duration_type& ttl=duration_type(-1)
                ): Base(uri, addresses, protocol, ttl){}
                explicit cs_north(
                    const std::string& uri,
                    addresses_type&& addresses,
                    const std::string& protocol=std::string(),
                    const duration_type& ttl=duration_type(-1)
                ): Base(uri, std::move(addresses), protocol, ttl){}
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
                    cs_south(std::string(), addresses_type()){}
                cs_south(const std::string& uri):
                    cs_south(uri, addresses_type()){}
                cs_south(const std::string& uri, const std::string& protocol):
                    cs_south(uri, addresses_type(), protocol){}
                cs_south(const struct sockaddr *addr, socklen_t addrlen, const std::string& protocol, const duration_type& ttl=duration_type(-1)):
                    cs_south(std::string(), addr, addrlen, protocol, ttl){}              
                cs_south(const addresses_type& addresses, const std::string& protocol, const duration_type& ttl=duration_type(-1)):
                    cs_south(std::string(), addresses, protocol, ttl){}
                cs_south(addresses_type&& addresses, const std::string& protocol, const duration_type& ttl=duration_type(-1)):
                    cs_south(std::string(), std::move(addresses), protocol, ttl){}
                explicit cs_south(
                    const std::string& uri,
                    const struct sockaddr *addr,
                    socklen_t addrlen,
                    const std::string& protocol,
                    const duration_type& ttl=duration_type(-1)
                ): Base(uri, addr, addrlen, protocol, ttl){}
                explicit cs_south(
                    const std::string& uri,
                    const addresses_type& addresses,
                    const std::string& protocol=std::string(),
                    const duration_type& ttl=duration_type(-1)
                ): Base(uri, addresses, protocol, ttl){}
                explicit cs_south(
                    const std::string& uri,
                    addresses_type&& addresses,
                    const std::string& protocol=std::string(),
                    const duration_type& ttl=duration_type(-1)
                ): Base(uri, std::move(addresses), protocol, ttl){}
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