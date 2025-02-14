/*     
*   Copyright 2025 Kevin Exton
*   This file is part of Cloudbus.
*
* Cloudbus is free software: you can redistribute it and/or modify it under the 
*   terms of the GNU General Public License as published by the Free Software 
*   Foundation, either version 3 of the License, or any later version.
*
* Cloudbus is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
*   without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
*   See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with Cloudbus. 
*   If not, see <https://www.gnu.org/licenses/>. 
*/
#include "proxy_interfaces.hpp"
namespace cloudbus{
    namespace proxy {
      cbus_interface::cbus_interface(): Base() {}
      cbus_interface::cbus_interface(const address_type addr, size_type addrlen): Base(addr, addrlen){}
      cbus_interface::cbus_interface(cbus_interface&& other): Base(std::move(other)){}
      cbus_interface& cbus_interface::operator=(cbus_interface&& other){
        Base::operator=(std::move(other));
        return *this;
      }
    }
}