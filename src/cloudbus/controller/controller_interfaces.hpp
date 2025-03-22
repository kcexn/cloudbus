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
#include "../../interfaces.hpp"
#pragma once
#ifndef CLOUDBUS_CONTROLLER_INTERFACES
#define CLOUDBUS_CONTROLLER_INTERFACES
namespace cloudbus{
    namespace controller {
        class cs_north : public ss_interface 
        {
            public:
                using Base = ss_interface;
                cs_north(const std::string& protocol=std::string(), const std::string& url=std::string()):
                    cs_north(addresses_type(), protocol, url){}
                explicit cs_north(
                    const struct sockaddr *addr,
                    socklen_t addrlen,
                    const std::string& protocol,
                    const std::string& uri=std::string(),
                    const duration_type& ttl=duration_type(-1)
                ): Base(addr, addrlen, protocol, uri, ttl){}
                explicit cs_north(
                    const addresses_type& addresses,
                    const std::string& protocol=std::string(),
                    const std::string& uri=std::string(),
                    const duration_type& ttl=duration_type(-1)
                ): Base(addresses, protocol, uri, ttl){}
                explicit cs_north(
                    addresses_type&& addresses,
                    const std::string& protocol=std::string(),
                    const std::string& uri=std::string(),
                    const duration_type& ttl=duration_type(-1)
                ): Base(std::move(addresses), protocol, uri, ttl){}

                ~cs_north() = default;

                cs_north(const cs_north& other) = delete;
                cs_north& operator=(const cs_north& other) = delete;
                cs_north(cs_north&& other) = delete;
                cs_north& operator=(cs_north&& other) = delete;
        };
        class cs_south : public cs_interface 
        {
            public:
                using Base = cs_interface;

                cs_south(const std::string& protocol=std::string(), const std::string& url=std::string()):
                    cs_south(addresses_type(), protocol, url){}
                explicit cs_south(
                    const struct sockaddr *addr,
                    socklen_t addrlen,
                    const std::string& protocol,
                    const std::string& uri=std::string(),
                    const duration_type& ttl=duration_type(-1)
                ): Base(addr, addrlen, protocol, uri, ttl){}
                explicit cs_south(
                    const addresses_type& addresses,
                    const std::string& protocol=std::string(),
                    const std::string& uri=std::string(),
                    const duration_type& ttl=duration_type(-1)
                ): Base(addresses, protocol, uri, ttl){}
                explicit cs_south(
                    addresses_type&& addresses,
                    const std::string& protocol=std::string(),
                    const std::string& uri=std::string(),
                    const duration_type& ttl=duration_type(-1)
                ): Base(std::move(addresses), protocol, uri, ttl){}

                ~cs_south() = default;

                cs_south(const cs_south& other) = delete;
                cs_south& operator=(const cs_south& other) = delete;
                cs_south(cs_south&& other) = delete;
                cs_south& operator=(cs_south&& other) = delete;
        };
    }
}
#endif