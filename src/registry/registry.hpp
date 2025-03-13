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
#include <string>
#include <tuple>
#include <variant>
#include <sys/socket.h>
#pragma once
#ifndef CLOUDBUS_REGISTRY
#define CLOUDBUS_REGISTRY
namespace cloudbus{
    namespace registry {
        enum types { URN, URL, SOCKADDR };
        using socket_address = std::tuple<std::string, struct sockaddr_storage, socklen_t>;
        using url = std::tuple<std::string, std::string>;
        using address_type = std::variant<std::string, url, socket_address>;
        /* unix:///<PATH> */
        /* tcp://<IP>:<PORT> */
        address_type make_address(const std::string& line);
    }
}
#endif