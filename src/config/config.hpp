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
#include <iostream>
#include <string>
#include <tuple>
#include <variant>
#include <vector>
#include <sys/socket.h>
#pragma once
#ifndef CLOUDBUS_CONFIG
#define CLOUDBUS_CONFIG
namespace cloudbus{
    namespace config {
        enum types { URN, URL, SOCKADDR };
        using socket_address = std::tuple<std::string, struct sockaddr_storage, socklen_t>;
        using url = std::tuple<std::string, std::string>;
        using address_type = std::variant<std::string, url, socket_address>;

        address_type make_address(const std::string& line);
        struct section {
            using kvp = std::tuple<std::string, std::string>;
            std::string heading;
            std::vector<kvp> config;
        };
        class configuration {
            public:
                configuration();
                configuration(const configuration& other);
                configuration(configuration&& other) noexcept;

                configuration& operator=(const configuration& other);
                configuration& operator=(configuration&& other) noexcept;

                const std::vector<section>& sections() const { return _sections; }

                virtual ~configuration() = default;

            private:
                std::vector<section> _sections;

                friend std::istream& operator>>(std::istream& is, configuration& config);
                friend std::ostream& operator<<(std::ostream& os, const configuration& config);
        };

    }
}
#endif
