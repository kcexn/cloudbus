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
#include "node.hpp"
#include <algorithm>
#include <fstream>
#ifdef COMPILE_CONTROLLER
    #include "cloudbus/controller/controller_connector.hpp"
#endif
#ifdef COMPILE_SEGMENT
    #include "cloudbus/segment/segment_connector.hpp"
#endif
#ifdef COMPILE_PROXY
    #include "cloudbus/proxy/proxy_connector.hpp"
#endif

int main(int argc, char* argv[]) {
    std::string path;
    #ifdef COMPILE_CONTROLLER
        path = "controller.ini";
    #endif
    #ifdef COMPILE_SEGMENT
        path = "segment.ini";
    #endif
    #ifdef COMPILE_PROXY
        path = "proxy.ini";
    #endif
    std::fstream f{path, f.in};
    cloudbus::config::configuration config;
    f >> config;
    for(const auto& section: config.sections()){
        std::string heading = section.heading;
        std::transform(heading.begin(), heading.end(), heading.begin(), [](const unsigned char c){ return std::toupper(c); });
        if(heading != "CLOUDBUS"){
            #ifdef COMPILE_CONTROLLER
                using controller = cloudbus::basic_node<cloudbus::controller::connector>;
                return controller(section).run();
            #endif
            #ifdef COMPILE_SEGMENT
                using segment = cloudbus::basic_node<cloudbus::segment::connector>;
                return segment(section).run();
            #endif
            #ifdef COMPILE_PROXY
                using proxy = cloudbus::basic_node<cloudbus::proxy::connector>;
                return proxy(section).run();
            #endif
        }
    }
    return 0;
}