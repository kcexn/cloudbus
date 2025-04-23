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
#include "manager.hpp"
#include <fstream>
int main(int argc, char* argv[]) {
    std::string path;
    #ifdef CONFDIR
        path = CONFDIR;
    #else
        path = ".";
    #endif
    #ifdef COMPILE_CONTROLLER
        path += "/controller.ini";
    #endif
    #ifdef COMPILE_SEGMENT
        path += "/segment.ini";
    #endif
    #ifdef COMPILE_PROXY
        path += "/proxy.ini";
    #endif
    cloudbus::config::configuration config;
    if(std::fstream f{path, f.in}; f.good())
        f >> config;
    else return -1;
    #ifdef COMPILE_CONTROLLER
        return cloudbus::basic_manager<cloudbus::controller_type>(config).run();
    #endif
    #ifdef COMPILE_SEGMENT
        return cloudbus::basic_manager<cloudbus::segment_type>(config).run();
    #endif
    #ifdef COMPILE_PROXY
        return cloudbus::basic_manager<cloudbus::proxy_type>(config).run();
    #endif
    return 0;
}