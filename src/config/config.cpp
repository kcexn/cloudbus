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
#include "config.hpp"
#include <algorithm>
#include <string_view>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <cstddef>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
namespace cloudbus{
    namespace config {
        enum hostname_types { HOSTNAME_ERROR, IPV4, IPV6, HOSTNAME };
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
        static std::string_view _ipv6(const char *c, std::size_t len){
            if(c == nullptr || *c != '[')
                return std::string_view();
            const char *start = c;
            for(; len-- > 0 && *c !=']'; ++c);
            if(len == SIZE_MAX)
                return std::string_view();
            else return std::string_view(start, c-start);
        }
        static std::string_view _host(const char *c, std::size_t len){
            if(c == nullptr) return std::string_view();
            const char *start = c;
            for(; len-- > 0 && !std::isspace(*c); ++c);
            if(len == SIZE_MAX) return std::string_view(start, ++c - start);
            else return std::string_view(start, c - start);
        }
        static hostname_types _hostname_type(const char *c, std::size_t len){
            if(c == nullptr)
                return HOSTNAME_ERROR;
            std::size_t period_count=0;
            for(const char *const start = c; len-- > 0; ++c){
                switch(*c){
                    case '.':
                        ++period_count;
                        break;
                    case '[':
                        if(c == start)
                            return IPV6;
                        else return HOSTNAME_ERROR;
                    case ':':
                        if(period_count==3)
                            return IPV4;
                        else return HOSTNAME;
                    default:
                        if(!std::isdigit(*c))
                            return HOSTNAME;
                        break;
                }
            }
            return HOSTNAME_ERROR;
        }
        static std::string_view _protocol(const char *c, std::size_t len){
            if(c == nullptr)
                return std::string_view();
            for(; len-- > 0 && std::isspace(*c); ++c);
            if(len == SIZE_MAX)
                return std::string_view();
            const char *start = c;
            for(; len-- > 0  && *c != ':'; ++c);
            if(len == SIZE_MAX)
                return std::string_view();
            return std::string_view(start, c - start);
        }
        static int make_unix_address(const std::string_view& path, struct sockaddr_un *addr, socklen_t *len){
            constexpr std::size_t MAXLEN = sizeof(struct sockaddr_un) - offsetof(struct sockaddr_un, sun_path) - 1;
            if(!path.size() || path.size() > MAXLEN)
                return -1;
            addr->sun_family = AF_UNIX;
            std::strncpy(addr->sun_path, path.data(), path.size());
            *len = offsetof(struct sockaddr_un, sun_path) + path.size();
            return 0;
        }
        static int make_ipv4_address(const std::string_view& host, struct sockaddr_in *addr, socklen_t *len){
            if(!host.size())
                return -1;
            auto ip = _ipv4(host.data(), host.size());
            if(!ip.size() || ip.size()+1 >= host.size())
                return -1;
            const char *start = ip.data()+ip.size()+1;
            std::size_t size = host.size() - (start-host.data());
            auto port = _port(start, size);
            if(port.size() == 0)
                return -1;
            addr->sin_family = AF_INET;
            if(inet_pton(addr->sin_family, std::string(ip).c_str(), &addr->sin_addr) < 1)
                return -1;
            auto [ptr, ec] = std::from_chars(port.data(), port.data() + port.size(), addr->sin_port);
            if(ec != std::errc())
                return -1;
            addr->sin_port = htons(addr->sin_port);
            *len = sizeof(struct sockaddr_in);
            return 0;
        }
        static int make_ipv6_address(const std::string_view& host, struct sockaddr_in6 *addr, socklen_t *len){
            if(!host.size())
                return -1;
            auto ip = _ipv6(host.data(), host.size());
            if(!ip.size() || ip.size()+3 >= host.size())
                return -1;
            const char *start = ip.data()+ip.size()+2;
            std::size_t size = host.size()-(start-host.data());
            auto port = _port(start, size);
            if(!port.size())
                return -1;
            addr->sin6_family = AF_INET6;
            if(inet_pton(addr->sin6_family, std::string(ip).c_str(), &addr->sin6_addr) < 1)
                return -1;
            auto[ptr, ec] = std::from_chars(port.data(), port.data()+port.size(), addr->sin6_port);
            if(ec != std::errc())
                return -1;
            addr->sin6_port = htons(addr->sin6_port);
            *len = sizeof(struct sockaddr_in6);
            return 0;
        }
        static address_type make_url(const std::string_view& host, std::string&& p){
            auto it = std::find(host.begin(), host.end(), ':');
            if(++it == host.end())
                return address_type();
            while(std::isdigit(*it++) && it < host.end());
            return (it==host.end()) ? address_type(url{host, std::move(p)}) : address_type();
        }
        /* unix:///<PATH> */
        /* tcp://<IP>:<PORT> */
        static address_type make_address(const std::string& line, std::string&& p){
            socket_address address{};
            auto&[protocol, addr, addrlen] = address;
            protocol = std::move(p);
            const char *start = line.data() + protocol.size() + 3;
            std::size_t size = line.size() - (start - line.data());
            if(protocol == "UNIX"){
                if(make_unix_address(_path(start, size), reinterpret_cast<struct sockaddr_un*>(&addr), &addrlen))
                    return address_type();
                return address_type{std::move(address)};
            } else if (protocol == "TCP" || protocol=="UDP" || protocol == "SCTP"){
                auto host = _host(start, size);
                if(!host.size())
                    return address_type();
                switch(_hostname_type(host.data(), host.size())){
                    case IPV4:
                        if(make_ipv4_address(host, reinterpret_cast<struct sockaddr_in*>(&addr), &addrlen))
                            return address_type();
                        return address_type{std::move(address)};
                    case IPV6:
                        if(make_ipv6_address(host, reinterpret_cast<struct sockaddr_in6*>(&addr), &addrlen))
                            return address_type();
                        return address_type{std::move(address)};
                    case HOSTNAME:
                        return make_url(host, std::move(protocol));
                    default:
                        return address_type();
                }
            } else return address_type();
        }
        static address_type make_urn(const std::string& line){
            auto it = std::find_if(line.cbegin(), line.cend(), [](const unsigned char c){ return std::isspace(c); });
            return address_type(std::string(line.cbegin(), it));
        }
        address_type make_address(const std::string& line){
            std::string protocol{_protocol(line.data(), line.size())};
            if(protocol.empty())
                return address_type();
            std::transform(protocol.begin(), protocol.end(), protocol.begin(), [](const unsigned char c){ return std::toupper(c); });
            if(protocol == "UNIX" || protocol=="TCP" || protocol=="UDP" || protocol == "SCTP")
                return make_address(line, std::move(protocol));
            else if(protocol == "URN")
                return make_urn(line);
            else return address_type();
        }

        configuration::configuration():
            _sections{}{}
        configuration::configuration(const configuration& other):
            _sections{other._sections}{}
        configuration::configuration(configuration&& other) noexcept:
            _sections(std::move(other._sections)){}
        configuration& configuration::operator=(const configuration& other){
            _sections = other._sections;
            return *this;
        }
        configuration& configuration::operator=(configuration&& other) noexcept
        {
            _sections = std::move(other._sections);
            return *this;
        }

        std::istream& operator>>(std::istream& is, configuration& conf){
            auto& sections = conf._sections;
            sections.clear();
            std::string line;
            while(std::getline(is, line)){
                auto start = std::find_if(line.begin(), line.end(), [](const unsigned char c){ return !std::isspace(c); });
                std::string_view entry(&*start, line.end()-start);
                if(entry.size() == 0)
                    continue;
                if(entry.front() == '['){
                    auto end = std::find(entry.cbegin()+1, entry.cend(), ']');
                    if(end == entry.cend())
                        continue;
                    while(std::isspace(*(--end)));
                    sections.emplace_back().heading = std::string(entry.cbegin()+1, ++end);
                } else if(!sections.empty()){
                    auto& section = sections.back();
                    auto& config = section.config;
                    auto delim = std::find(entry.cbegin(), entry.cend(), '=');
                    if(delim == entry.cend())
                        continue;
                    auto sval = std::find_if(delim+1, entry.cend(), [](const unsigned char c){ return !std::isspace(c); });
                    if(sval == entry.cend())
                        continue;
                    auto end = entry.cend();
                    while(std::isspace(*(--end)));
                    while(std::isspace(*(--delim)));
                    config.emplace_back(std::string(entry.cbegin(), ++delim), std::string(sval, ++end));
                }
            }
            return is;
        }
        std::ostream& operator<<(std::ostream& os, const configuration& config){
            for(auto& section: config._sections){
                os << '[' << section.heading << "]\n";
                for(auto&[key, value]: section.config)
                    os << key << '=' << value << '\n';
                os << '\n';
            }
            return os;
        }
    }
}