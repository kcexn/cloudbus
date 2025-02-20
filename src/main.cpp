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
#ifdef COMPILE_CONTROLLER
    #include "controller/controller.hpp"
#endif
#ifdef COMPILE_SEGMENT
    #include "segment/segment.hpp"
#endif
#ifdef COMPILE_PROXY
    #include "proxy/proxy.hpp"
#endif

int main(int argc, char* argv[]) {
    #ifdef COMPILE_CONTROLLER
        return cloudbus::controller::controller().run();
    #endif
    #ifdef COMPILE_SEGMENT
        return cloudbus::segment::segment().run();
    #endif
    #ifdef COMPILE_PROXY
        return cloudbus::proxy::proxy().run();
    #endif
    return 0;
}