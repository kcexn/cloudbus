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
#include "config.hpp"
#include <array>
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
            if(c == nullptr)
                return std::string_view();
            /* strip leading whitespace. */
            while(len-- > 0 && std::isspace(*c++)); 
            if(len++==SIZE_MAX)
                return std::string_view();
            const char *start = --c;
            /* Then take everything up to the first whitespace. */
            while(len-- > 0 && !std::isspace(*c++));
            return (len==SIZE_MAX) ?
                std::string_view(start, c-start) :
                std::string_view(start, --c-start);
        }
        static std::string_view _port(const char *c, std::size_t len){
            if(c == nullptr)
                return std::string_view();
            const char *start = c;
            while(len-- > 0 && std::isdigit(*c++));
            return (len==SIZE_MAX) ?
                std::string_view(start, c-start) :
                std::string_view(start, --c-start);
        }
        static std::string_view _ipv4(const char *c, std::size_t len){
            if(c == nullptr)
                return std::string_view();
            const char *start = c;
            while(len-- > 0 && *c++ != ':');
            return (len==SIZE_MAX) ?
                std::string_view() :
                std::string_view(start, --c-start);
        }
        static std::string_view _ipv6(const char *c, std::size_t len){
            if(c == nullptr || *c++ != '[')
                return std::string_view();
            const char *start = c;
            while(len-- > 0 && *c++ != ']');
            return (len==SIZE_MAX) ?
                std::string_view() :
                std::string_view(start, --c-start);
        }
        static std::string_view _host(const char *c, std::size_t len){
            if(c == nullptr)
                return std::string_view();
            const char *start = c;
            while(len-- > 0 && !std::isspace(*c++));
            return (len==SIZE_MAX) ?
                std::string_view(start, c-start) :
                std::string_view(start, --c-start);
        }
        static hostname_types _hostname_type(const char *c, std::size_t len){
            if(c == nullptr)
                return HOSTNAME_ERROR;
            std::size_t period_count=0;
            const char *start=c, *cur=start;
            while(len-- > 0 && (cur=c++)){
                switch(*cur){
                    case '.':
                        ++period_count;
                        break;
                    case '[':
                        if(cur == start)
                            return IPV6;
                        else return HOSTNAME_ERROR;
                    case ':':
                        if(period_count==3)
                            return IPV4;
                        else return HOSTNAME;
                    default:
                        if(!std::isdigit(*cur))
                            return HOSTNAME;
                        break;
                }
            }
            return HOSTNAME_ERROR;
        }
        static std::string_view _protocol(const char *c, std::size_t len){
            if(c == nullptr)
                return std::string_view();
            while(len-- > 0 && std::isspace(*c++));
            if(len++ == SIZE_MAX)
                return std::string_view();
            /* RFC 3986 (https://datatracker.ietf.org/doc/html/rfc3986#section-3.1) specifies that *
             * a URI scheme must be of the form: ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )        */
            const char *start=--c, *cur=start;
            if(!std::isalpha(*start))
                return std::string_view();
            while(len-- > 0 && *(cur=c++) != ':')
                if( !(std::isalpha(*cur) || std::isdigit(*cur) || *cur=='+' || *cur=='-' || *cur=='.') )
                    return std::string_view();
            return (len==SIZE_MAX) ?
                std::string_view() :
                std::string_view(start, --c-start);
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
            if(it == host.end() || ++it == host.end())
                return address_type();
            /* There must be at least one digit. */
            while(std::isdigit(*it) && ++it != host.end());
            return (it==host.end()) ?
                address_type(url_type{host, std::move(p)}) :
                address_type();
        }
        static address_type make_address(const std::string& line, std::string&& p){
            socket_address address{};
            auto&[protocol, addr, addrlen] = address;
            protocol = std::move(p);
            const char *start = line.data() + protocol.size() + 3;
            std::size_t size = line.size() - (start - line.data());
            if(protocol == "UNIX") {
                if(make_unix_address(_path(start, size), reinterpret_cast<struct sockaddr_un*>(&addr), &addrlen))
                    return address_type();
                return address_type{std::move(address)};
            } else if (protocol == "TCP" || protocol=="UDP" || protocol == "SCTP") {
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
            }
            return address_type();
        }
        address_type make_address(const std::string& line){
            std::string protocol{_protocol(line.data(), line.size())};
            if(!protocol.empty()){
                std::transform(
                        protocol.begin(),
                        protocol.end(),
                    protocol.begin(),
                    [](const unsigned char c){
                        return std::toupper(c);
                    }
                );
                if(protocol == "UNIX" || protocol == "TCP" || 
                    protocol == "UDP" || protocol == "SCTP"
                ){
                    return make_address(line, std::move(protocol));
                }
                return address_type( uri_type(_path(line.data(), line.size())) );
            }
            return address_type();
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
        static void parse_line(
            std::string& line,
            std::string& heading,
            configuration::sections_type& sections
        ){
            auto start = line.cbegin(), end = line.cend();
            start = std::find_if(start, end,
                [](unsigned char c){
                    return !std::isspace(c);
                }
            );
            if(start != end) {
                while(std::isspace(*--end));
                if(*start == '[' && *end == ']') {
                    start = std::find_if(++start, end,
                        [](unsigned char c){
                            return !std::isspace(c);
                        }
                    );
                    if(start != end) {
                        while(std::isspace(*--end));
                        heading = std::string(start, ++end);
                        sections.try_emplace(heading);
                    }
                } else if(!sections.empty()) {
                    auto& section = sections.at(heading);
                    auto key_end = std::find(start, ++end, '=');
                    if(key_end != end) {
                        auto value_start = std::find_if(key_end+1, end,
                            [](unsigned char c) {
                                return !std::isspace(c);
                            }
                        );
                        if(value_start != end) {
                            while(std::isspace(*--key_end));
                            section.emplace_back(std::string(start, ++key_end), std::string(value_start, end));
                        }
                    }
                }
            }
            line.clear();
        }
        std::istream& operator>>(std::istream& is, configuration& conf) {
            static constexpr std::streamsize buflen = 256;
            auto& sections = conf._sections;
            sections.clear();
            std::string line, heading;
            std::array<char, buflen> buf{};
            do {
                is.read(buf.data(), buf.max_size());
                auto begin=buf.begin(), end=begin+is.gcount();
                for(auto c=begin; c != end; ++c) {
                    if(!std::isspace(*c)) {
                        if(!line.empty() && line.back() == '\n')
                            parse_line(line, heading, sections);
                        line.push_back(*c);
                    } else switch(*c) {
                        case ' ':
                        case '\t':
                            if(line.empty() || !std::isspace(line.back())) {
                                line.push_back(' ');
                            } else {
                                line.back() = ' ';
                            }
                            break;
                        case '\n':
                            if(line.empty() || !std::isspace(line.back())) {
                                line.push_back('\n');
                            } else if(line.back() == '\n') {
                                parse_line(line, heading, sections);
                            } else {
                                line.back() = '\n';
                            }
                        default:
                            break;
                    }
                }
            } while(is.good());
            if(!line.empty() && line.back() == '\n')
                parse_line(line, heading, sections);
            return is;
        }
        std::ostream& operator<<(std::ostream& os, const configuration& config){
            for(auto&[heading, section]: config._sections){
                os << '[' << heading << "]\n";
                for(auto&[key, value]: section)
                    os << key << '=' << value << '\n';
                os << '\n';
            }
            return os;
        }
    }
}
