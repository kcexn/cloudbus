/*     
*   Copyright 2024 Kevin Exton
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
#include "messages.hpp"
#include <fstream>
#include <iostream>
#include <cstdint>
namespace cloudbus{
    namespace messages{
        uuid make_uuid_v4(){
            std::ifstream urandom("/dev/urandom", std::ios::in|std::ios::binary);       
            const std::uint8_t VARIANT_MASK = 0xBF;
            const std::uint16_t VERSION_MASK = 0x4FFF;
            uuid uuid;
            urandom.read(reinterpret_cast<char*>(&uuid), 16);
            uuid.clock_seq_hi_and_reserved |= VARIANT_MASK;
            uuid.clock_seq_hi_and_reserved &= VARIANT_MASK;
            uuid.time_hi_and_version |= VERSION_MASK;
            uuid.time_hi_and_version &= VERSION_MASK;
            return uuid;
        }
    }
}
