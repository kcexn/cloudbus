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
#include "registry.hpp"
#include <string_view>
#include <charconv>
#include <cctype>
#include <cstring>
#include <cstddef>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
namespace cloudbus{
    namespace registry {
        static std::string_view _path(const char *c, std::size_t len){
            if(c == nullptr) return std::string_view();
            const char *start = c;
            for(; len-- > 0 && !std::isspace(*c); ++c);
            if(len == SIZE_MAX) return std::string_view(start, ++c - start);
            else return std::string_view(start, c - start);
        }
        static std::string_view _port(const char *c, std::size_t len){
            if(c == nullptr) return std::string_view();
            const char *start = c;
            for(; len-- > 0 && std::isdigit(*c); ++c);
            if(len == SIZE_MAX) return std::string_view(start, ++c - start);
            else return std::string_view(start, c - start);
        }
        static std::string_view _ipv4(const char *c, std::size_t len){
            if(c == nullptr) return std::string_view();
            const char *start = c;
            for(; len-- > 0 && *c != ':'; ++c);
            if(len == SIZE_MAX) return std::string_view();
            else return std::string_view(start, c - start);
        }
        static std::string_view _host(const char *c, std::size_t len){
            if(c == nullptr) return std::string_view();
            const char *start = c;
            for(; len-- > 0 && !std::isspace(*c); ++c);
            if(len == SIZE_MAX) return std::string_view(start, ++c - start);
            else return std::string_view(start, c - start);
        }
        static std::string_view _protocol(const char *c, std::size_t len){
            if(c == nullptr) return std::string_view();
            for(; len-- > 0 && std::isspace(*c); ++c);
            if(len == SIZE_MAX) return std::string_view();
            const char *start = c;
            for(; len-- > 0  && *c != ':'; ++c);
            if(len == SIZE_MAX) return std::string_view();
            return std::string_view(start, c - start);
        }
        static int make_unix_address(std::string_view path, struct sockaddr_un *addr, socklen_t *len){
            constexpr std::size_t maxlen = sizeof(struct sockaddr_un) - offsetof(struct sockaddr_un, sun_path) - 1;
            if(path.size() > maxlen || path.size() == 0) return -1;
            addr->sun_family = AF_UNIX;
            std::strncpy(addr->sun_path, path.data(), path.size());
            *len = offsetof(struct sockaddr_un, sun_path) + path.size();
            return 0;
        }
        static int make_ipv4_address(std::string_view host, struct sockaddr_in *addr, socklen_t *len){
            if(host.size() == 0) return -1;
            auto ip = _ipv4(host.data(), host.size());
            if(ip.size() == 0) return -1;
            const char *start = ip.data() + ip.size() + 1;
            std::size_t size = host.size() - (start - host.data());
            auto port = _port(start, size);
            if(port.size() == 0) return -1;
            addr->sin_family = AF_INET;
            auto [ptr, ec] = std::from_chars(port.data(), port.data() + port.size(), addr->sin_port);
            if(ec != std::errc()) return -1;
            addr->sin_port = htons(addr->sin_port);
            if(inet_pton(addr->sin_family, std::string(ip).c_str(), &addr->sin_addr) < 1) return -1;
            *len = sizeof(struct sockaddr_in);
            return 0;
        }
        /* unix:///<PATH> */
        /* tcp://<IP>:<PORT> */        
        int make_address(const std::string& line, struct sockaddr *addr, socklen_t *len, transport& protocol){
            auto t = _protocol(line.data(), line.size());
            const char *start = t.data() + t.size() + 3;
            std::size_t size = line.size() - (start - line.data());
            if(t == "unix"){
                protocol = transport::UNIX;
                return make_unix_address(_path(++start, --size), reinterpret_cast<struct sockaddr_un*>(addr), len);
            } else if(t == "tcp"){
                protocol = transport::TCP;
                return make_ipv4_address(_host(start, size), reinterpret_cast<struct sockaddr_in*>(addr), len);
            }
            return -1;
        }
    }
}