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
#include "connectors.hpp"
#include <fcntl.h>
namespace cloudbus {
    static int set_flags(interface_base::native_handle_type fd){
        int flags = 0;
        if(fcntl(fd, F_SETFD, FD_CLOEXEC))
            throw std::runtime_error("Unable to set the cloexec flag.");
        if((flags = fcntl(fd, F_GETFL)) == -1)
            throw std::runtime_error("Unable to get file flags.");
        if(fcntl(fd, F_SETFL, flags | O_NONBLOCK))
            throw std::runtime_error("Unable to set the socket to nonblocking mode.");
        return fd;
    }    
    connector_base::connector_base(int mode):
        _north{}, _south{}, _connections{}, _mode{mode}{}

    interface_base::native_handle_type connector_base::make_north(const registry::address_type& address){
        if(address.index() != registry::SOCKADDR)
            return -1;
        auto&[protocol, addr, addrlen] = std::get<registry::SOCKADDR>(address);
        _north.push_back(std::make_shared<interface_base>(reinterpret_cast<const struct sockaddr*>(&addr), addrlen, protocol));
        if(protocol == "TCP" || protocol == "UNIX"){
            auto& hnd = _north.back()->make(addr.ss_family, SOCK_STREAM, 0);
            auto& sockfd = std::get<interface_base::native_handle_type>(*hnd);
            set_flags(sockfd);
            int reuse = 1;
            setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
            if(bind(sockfd, reinterpret_cast<const struct sockaddr*>(&addr), addrlen))
                throw std::runtime_error("bind()");
            if(listen(sockfd, 128))
                throw std::runtime_error("listen()");
            return sockfd;
        }
        return -1;
    }

    int connector_base::make_south(const registry::address_type& address){
        switch(address.index()){
            case registry::SOCKADDR:
            {
                auto&[protocol, addr, addrlen] = std::get<registry::SOCKADDR>(address);
                _south.push_back(std::make_shared<interface_base>(reinterpret_cast<const struct sockaddr*>(&addr), addrlen, protocol));
                return 0;
            }
            case registry::URL:
            {
                auto&[host, protocol] = std::get<registry::URL>(address);
                _south.push_back(std::make_shared<interface_base>(host, protocol));
                return 0;
            }
            case registry::URN:
                if(auto& urn = std::get<registry::URN>(address); !urn.empty()){
                    _south.push_back(std::make_shared<interface_base>(urn));
                    return 0;
                }
            default:
                return -1;
        }
        return -1;
    }
}