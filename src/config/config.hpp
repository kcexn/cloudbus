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
#include <iostream>
#include <string>
#include <tuple>
#include <variant>
#include <vector>
#include <map>
#include <sys/socket.h>
#pragma once
#ifndef CLOUDBUS_CONFIG
#define CLOUDBUS_CONFIG
namespace cloudbus{
    namespace config {
        /* URI's must always be of the form <SCHEME>:<SCHEME SPECIFIC PART>. *
         * Malformed URI's will be rejected.                                 */
        enum types { URI, URL, SOCKADDR };
        using socket_address = std::tuple<std::string, struct sockaddr_storage, socklen_t>;
        using url_type = std::tuple<std::string, std::string>;
        using uri_type = std::string;
        using address_type = std::variant<uri_type, url_type, socket_address>;

        address_type make_address(const std::string& line);
        using kvp = std::pair<std::string, std::string>;
        using section = std::vector<kvp>;
        class configuration {
            public:
                using sections_type = std::map<std::string, section>;
                configuration();
                configuration(const configuration& other);
                configuration(configuration&& other) noexcept;

                configuration& operator=(const configuration& other);
                configuration& operator=(configuration&& other) noexcept;

                const sections_type& sections() const { return _sections; }

                virtual ~configuration() = default;

            private:
                sections_type _sections;

                friend std::istream& operator>>(std::istream& is, configuration& config);
                friend std::ostream& operator<<(std::ostream& os, const configuration& config);
        };

    }
}
#endif
