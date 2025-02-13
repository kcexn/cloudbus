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
#include "control_interfaces.hpp"
#include <algorithm>
#include <sys/un.h>
namespace cloudbus{
    namespace controller{
        cs_north::cs_north(): Base() {}
        cs_north::cs_north(const address_type addr, size_type addr_len): Base(addr, addr_len){}
        cs_north::cs_north(cs_north&& other): Base(std::move(other)){}
        cs_north& cs_north::operator=(cs_north&& other){
            Base::operator=(std::move(other));
            return *this;
        }
        cs_south::cs_south(): Base(){}
        cs_south::cs_south(const address_type addr, size_type addr_len): Base(addr, addr_len){}
        cs_south::cs_south(cs_south&& other): Base(std::move(other)){}
        cs_south& cs_south::operator=(cs_south&& other){
            Base::operator=(std::move(other));
            return *this;
        }
    }
}