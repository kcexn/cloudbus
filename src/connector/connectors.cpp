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
    connector_base::connector_base(
        const config::section& section,
        int mode
    ):
        _north{}, _south{}, _connections{},
        _mode{mode}, _drain{0}
    {
        short dir=0;
        interface_base::options_type soptions, noptions;
        for(const auto&[key, value]: section){
            std::string k = key;
            std::transform(k.begin(), k.end(), k.begin(), [](const unsigned char c){ return std::toupper(c); });
            if(k == "BIND") {
                if(auto nfd = make_north(config::make_address(value)); nfd < 0)
                    throw std::invalid_argument("Invalid bind address.");
                dir = 1;
            } else if(k == "BACKEND"){
                if(make_south(config::make_address(value)))
                    throw std::invalid_argument("Invalid backend address.");
                dir = -1;
            } else if(k == "MODE") {
                std::string v = value;
                std::transform(v.begin(), v.end(), v.begin(), [](const unsigned char c){ return std::toupper(c); });
                if(v == "FULL_DUPLEX")
                    _mode = FULL_DUPLEX;
            } else {
                if(!dir)
                    continue;
                else if(dir > 0)
                    noptions.emplace_back(k, value);
                else if(dir < 0)
                    soptions.emplace_back(k, value);
            }
        }
        if(north().empty())
            throw std::invalid_argument("A service must be configued with a bind address.");
        if(south().empty())
            throw std::invalid_argument("A service must be configured with at least one backend.");
        for(auto& n: north())
            n.options() = noptions;
        for(auto& s: south())
            s.options() = soptions;
        if(_mode == FULL_DUPLEX && south().size() > messages::CLOCK_SEQ_MAX)
            throw std::invalid_argument("The service fanout ratio will overflow the UUID clock_seq.");
    }
    interface_base::native_handle_type connector_base::make_north(const config::address_type& address){
        if(address.index() != config::SOCKADDR)
            return -1;
        auto&[protocol, s_addr, addrlen] = std::get<config::SOCKADDR>(address);
        const auto *addr = reinterpret_cast<const struct sockaddr*>(&s_addr);
        _north.emplace_back(addr, addrlen, protocol);
        if(protocol == "TCP" || protocol == "UNIX"){
            auto& hnd = _north.back().make(addr->sa_family, SOCK_STREAM, 0, std::ios_base::openmode());
            auto& sockfd = std::get<interface_base::native_handle_type>(hnd);
            set_flags(sockfd);
            if(protocol == "TCP") {
                int reuseaddr = 1;
                if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr))) {
                    throw std::system_error(
                        std::error_code(errno, std::system_category()),
                        "Unable to set SO_REUSEADDR."
                    );
                }
            }
            if(bind(sockfd, addr, addrlen))
                throw std::runtime_error("bind()");
            if(listen(sockfd, 128))
                throw std::runtime_error("listen()");
            return sockfd;
        }
        return -1;
    }
    static int make_south_sockaddr(connector_base::interfaces& south, const config::socket_address& sockaddr) {
        const auto&[protocol, addr, addrlen] = sockaddr;
        south.emplace_back(reinterpret_cast<const struct sockaddr*>(&addr), addrlen, protocol);
        return 0;
    }
    static int make_south_url(connector_base::interfaces& south, const config::url_type& url) {
        const auto&[host, protocol] = url;
        south.emplace_back(host, protocol);
        return 0;
    }
    static int make_south_uri(connector_base::interfaces& south, const config::uri_type& uri) {
        south.emplace_back(uri);
        return 0;
    }
    int connector_base::make_south(const config::address_type& address) {
        using namespace config;
        switch(address.index()) {
            case SOCKADDR:
                return make_south_sockaddr(_south, std::get<SOCKADDR>(address));
            case URL:
                return make_south_url(_south, std::get<URL>(address));
            case URI:
                return make_south_uri(_south, std::get<URI>(address));
            default:
                return -1;
        }
    }
    connector_base::~connector_base(){
        for(auto& n: north())
            for(const auto&[addr, addrlen, ttl, weight]: n.addresses())
                if(addr.ss_family == AF_UNIX)
                    unlink(reinterpret_cast<const struct sockaddr_un*>(&addr)->sun_path);
    }
}