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
#include "options.hpp"
#include <fstream>
#include <cstdlib>
static void throw_system_error(const std::string& what){
    throw std::system_error(
        std::error_code(errno, std::system_category()),
        what
    );
}
static void throw_invalid_argument(const std::string& what){
    throw std::invalid_argument(what);
}
int main(int argc, char* argv[]) {
    const char *path = nullptr;
    if(cloudbus::options::parse(argc, argv)) {
        return 0;
    } else if( !(path = std::getenv("CONFIG_PATH")) ) {
        #ifdef CONFDIR
            #ifdef COMPILE_CONTROLLER
                path = CONFDIR "/controller.ini";
            #elif defined(COMPILE_SEGMENT)
                path = CONFDIR "/segment.ini";
            #endif
        #else
            #ifdef COMPILE_CONTROLLER
                path = "controller.ini";
            #elif defined(COMPILE_SEGMENT)
                path = "segment.ini";
            #else
                throw_invalid_argument("Invalid Cloudbus Component.");
            #endif
        #endif
        if(setenv("CONFIG_PATH", path, 1))
            throw_system_error("Unable to set CONFIG_PATH.");
    }
    cloudbus::config::configuration config;
    if(std::fstream f{path, f.in}; f.good()) {
        f >> config;
    } else {
        throw_invalid_argument("Unable to open file: " + std::string(path));
    }
    #ifdef COMPILE_CONTROLLER
        return cloudbus::basic_manager<cloudbus::controller_type>(config).run();
    #elif defined(COMPILE_SEGMENT)
        return cloudbus::basic_manager<cloudbus::segment_type>(config).run();
    #else
        throw_invalid_argument("Invalid Cloudbus Component.");
    #endif
    return 0;
}