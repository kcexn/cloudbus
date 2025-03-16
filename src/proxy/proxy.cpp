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
#include "proxy.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fstream>
#include <csignal>
namespace cloudbus{
    namespace proxy{
        proxy::~proxy() {
            for(auto& n: connector().north())
                for(const auto&[addr, addrlen]: n->addresses())
                    if(addr.ss_family == AF_UNIX)
                        unlink(reinterpret_cast<const struct sockaddr_un*>(&addr)->sun_path);
        }
        int proxy::_signal_handler(std::uint64_t signal){
            if(signal & (SIGTERM | SIGHUP)){
                if(connector().connections().empty())
                    return signal & (SIGTERM | SIGHUP);
                timeout() = duration_type(50);
                connector().drain() = 1;
            }
            return 0;
        }
    }
}