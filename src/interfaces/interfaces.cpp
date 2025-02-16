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
#include "interfaces.hpp"
namespace cloudbus {
    cs_interface::cs_interface(const address_type addr, size_type addrlen)
    : Base(addr, addrlen){}
    cs_interface::cs_interface(cs_interface&& other)
    : Base(std::move(other)){}
    cs_interface& cs_interface::operator=(cs_interface&& other){
        Base::operator=(std::move(other));
        return *this;
    }

    ss_interface::ss_interface()
    : Base(){}
    ss_interface::ss_interface(const address_type addr, size_type addrlen)
    : Base(addr, addrlen){}
    ss_interface::ss_interface(ss_interface&& other)
    : Base(std::move(other)){}
    ss_interface& ss_interface::operator=(ss_interface&& other){
        Base::operator=(std::move(other));
        return *this;
    }
}