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
        class cbus_interface : public cs_interface {
            public:
                using Base = cs_interface;

                cbus_interface():
                    cbus_interface(std::string(), nullptr, 0){}
                cbus_interface(const std::string& uri):
                    cbus_interface(uri, nullptr, 0){}
                cbus_interface(const struct sockaddr *addr, socklen_t addrlen):
                    cbus_interface(std::string(), addr, addrlen){}
                cbus_interface(const addresses_type& addresses):
                    cbus_interface(std::string(), addresses){}
                cbus_interface(addresses_type&& addresses):
                    cbus_interface(std::string(), std::move(addresses)){}
                explicit cbus_interface(const std::string& uri, const struct sockaddr *addr, socklen_t addrlen):
                    Base(uri, addr, addrlen){}
                explicit cbus_interface(const std::string& uri, const addresses_type& addresses):
                    Base(uri, addresses){}
                explicit cbus_interface(const std::string& uri, addresses_type&& addresses):
                    Base(uri, std::move(addresses)){}
                explicit cbus_interface(cbus_interface&& other):
                    Base(std::move(other)){}
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
                    service_interface(std::string(), nullptr, 0){}
                service_interface(const std::string& uri):
                    service_interface(uri, nullptr, 0){}
                service_interface(const struct sockaddr *addr, socklen_t addrlen):
                    service_interface(std::string(), addr, addrlen){}
                service_interface(const addresses_type& addresses):
                    service_interface(std::string(), addresses){}
                service_interface(addresses_type&& addresses):
                    service_interface(std::string(), std::move(addresses)){}
                explicit service_interface(const std::string& uri, const struct sockaddr *addr, socklen_t addrlen):
                    Base(uri, addr, addrlen){}
                explicit service_interface(const std::string& uri, const addresses_type& addresses):
                    Base(uri, addresses){}
                explicit service_interface(const std::string& uri, addresses_type&& addresses):
                    Base(uri, std::move(addresses)){}
                explicit service_interface(service_interface&& other):
                    Base(std::move(other)){}
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