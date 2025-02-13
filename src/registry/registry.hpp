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
#include <string>
#include <sys/socket.h>
#pragma once
#ifndef CLOUDBUS_REGISTRY
#define CLOUDBUS_REGISTRY
namespace cloudbus{
    namespace registry {
        enum struct transport { UNKNOWN, UNIX, TCP };
        /* unix:///<PATH> */
        /* tcp://<IP>:<PORT> */
        int make_address(const std::string& line, struct sockaddr *addr, socklen_t *len, transport& protocol);
    }
}
#endif