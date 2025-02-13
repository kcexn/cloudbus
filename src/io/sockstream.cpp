/*     
*   Copyright 2024 Kevin Exton
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
#include "streams.hpp"
namespace io {
    namespace streams {
        sockstream& sockstream::operator=(sockstream&& other){
            _buf = std::move(other._buf);
            Base::operator=(std::move(other));
            return *this;
        }

        void sockstream::swap(sockstream& other){
            auto tmp = std::move(other._buf);
            other._buf = std::move(tmp);
            _buf = std::move(tmp);
            Base::swap(other);
        }
    }
}

