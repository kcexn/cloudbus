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
#include <unistd.h>
#include <sys/un.h>
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
    connector_base::connector_base(int mode, const config::configuration::section& section):
        _north{}, _south{}, _connections{}, 
        _mode{mode}, _drain{0}
    {
        std::string heading = section.heading;
        std::transform(heading.begin(), heading.end(), heading.begin(), [](const unsigned char c){ return std::toupper(c); });
        if(heading != "CLOUDBUS"){
            for(const auto&[key, value]: section.config){
                std::string k = key;
                std::transform(k.begin(), k.end(), k.begin(), [](const unsigned char c){ return std::toupper(c); });
                if(k == "BIND"){
                    if(auto nfd = make_north(config::make_address(value)); nfd < 0)
                        throw std::invalid_argument("Invalid bind address.");
                } else if(k == "SERVER"){
                    if(make_south(config::make_address(value)))
                        throw std::invalid_argument("Invalid server address.");
                } else if(k == "MODE"){
                    std::string v = value;
                    std::transform(v.begin(), v.end(), v.begin(), [](const unsigned char c){ return std::toupper(c); });
                    if(v == "FULL DUPLEX")
                        _mode = FULL_DUPLEX;
                }
            }
            if(_mode == FULL_DUPLEX && south().size() > messages::CLOCK_SEQ_MAX)
                throw std::invalid_argument("The service fanout ratio will overflow the UUID clock_seq.");
        } else throw std::invalid_argument("A connector must be initialized with a service configuration.");
    }
    interface_base::native_handle_type connector_base::make_north(const config::address_type& address){
        if(address.index() != config::SOCKADDR)
            return -1;
        auto&[protocol, addr, addrlen] = std::get<config::SOCKADDR>(address);
        _north.push_back(std::make_shared<interface_base>(reinterpret_cast<const struct sockaddr*>(&addr), addrlen, protocol));
        if(protocol == "TCP" || protocol == "UNIX"){
            auto& hnd = _north.back()->make(addr.ss_family, SOCK_STREAM, 0);
            auto& sockfd = std::get<interface_base::native_handle_type>(*hnd);
            set_flags(sockfd);
            if(bind(sockfd, reinterpret_cast<const struct sockaddr*>(&addr), addrlen))
                throw std::runtime_error("bind()");
            if(listen(sockfd, 128))
                throw std::runtime_error("listen()");
            return sockfd;
        }
        return -1;
    }

    int connector_base::make_south(const config::address_type& address){
        switch(address.index()){
            case config::SOCKADDR:
            {
                auto&[protocol, addr, addrlen] = std::get<config::SOCKADDR>(address);
                _south.push_back(std::make_shared<interface_base>(reinterpret_cast<const struct sockaddr*>(&addr), addrlen, protocol));
                return 0;
            }
            case config::URL:
            {
                auto&[host, protocol] = std::get<config::URL>(address);
                _south.push_back(std::make_shared<interface_base>(host, protocol));
                return 0;
            }
            case config::URN:
                if(auto& urn = std::get<config::URN>(address); !urn.empty()){
                    _south.push_back(std::make_shared<interface_base>(urn));
                    return 0;
                }
            default:
                return -1;
        }
        return -1;
    }

    connector_base::~connector_base(){
        for(auto& n: north())
            for(const auto&[addr, addrlen]: n->addresses())
                if(addr.ss_family == AF_UNIX)
                    unlink(reinterpret_cast<const struct sockaddr_un*>(&addr)->sun_path);
    }
}