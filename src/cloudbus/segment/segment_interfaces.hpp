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
#include "../../interfaces.hpp"
#pragma once
#ifndef CLOUDBUS_SEGMENT_INTERFACES
#define CLOUDBUS_SEGMENT_INTERFACES
namespace cloudbus{
    namespace segment {
        class cbus_interface : public cs_interface {
            public:
                using Base = cs_interface;
                cbus_interface():
                    cbus_interface(addresses_type()){}
                cbus_interface(const std::string& urn):
                    cbus_interface(addresses_type(), urn){}
                cbus_interface(const std::string& url, const std::string& protocol):
                    cbus_interface(addresses_type(), url, protocol){}
                explicit cbus_interface(
                    const struct sockaddr *addr,
                    socklen_t addrlen,
                    const std::string& protocol,
                    const std::string& uri=std::string(),
                    const duration_type& ttl=duration_type(-1)
                ): Base(addr, addrlen, protocol, uri, ttl){}
                explicit cbus_interface(
                    const addresses_type& addresses,
                    const std::string& protocol=std::string(),
                    const std::string& uri=std::string(),
                    const duration_type& ttl=duration_type(-1)
                ): Base(addresses, protocol, uri, ttl){}
                explicit cbus_interface(
                    addresses_type&& addresses,
                    const std::string& protocol=std::string(),
                    const std::string& uri=std::string(),
                    const duration_type& ttl=duration_type(-1)
                ): Base(std::move(addresses), protocol, uri, ttl){}
                cbus_interface& operator=(cbus_interface&& other){
                    Base::operator=(std::move(other));
                    return *this;
                }

                ~cbus_interface() = default;

                cbus_interface(const cbus_interface& other) = delete;
                cbus_interface& operator=(const cbus_interface& other) = delete;                        
        };
        class service_interface : public ss_interface {
            public:
                using Base = ss_interface;
                service_interface():
                    service_interface(addresses_type()){}
                service_interface(const std::string& urn):
                    service_interface(addresses_type(), urn){}
                service_interface(const std::string& url, const std::string& protocol):
                    service_interface(addresses_type(), url, protocol){}
                explicit service_interface(
                    const struct sockaddr *addr,
                    socklen_t addrlen,
                    const std::string& protocol,
                    const std::string& uri=std::string(),
                    const duration_type& ttl=duration_type(-1)
                ): Base(addr, addrlen, protocol, uri, ttl){}
                explicit service_interface(
                    const addresses_type& addresses,
                    const std::string& protocol=std::string(),
                    const std::string& uri=std::string(),
                    const duration_type& ttl=duration_type(-1)
                ): Base(addresses, protocol, uri, ttl){}
                explicit service_interface(
                    addresses_type&& addresses,
                    const std::string& protocol=std::string(),
                    const std::string& uri=std::string(),
                    const duration_type& ttl=duration_type(-1)
                ): Base(std::move(addresses), protocol, uri, ttl){}
                service_interface& operator=(service_interface&& other){
                    Base::operator=(std::move(other));
                    return *this;
                }

                ~service_interface() = default;

                service_interface(const service_interface& other) = delete;
                service_interface& operator=(const service_interface& other) = delete;                    
        };
    }
}
#endif