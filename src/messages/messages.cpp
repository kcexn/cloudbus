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
#include <chrono>
#include <cstdint>
namespace cloudbus{
    namespace messages {
        static constexpr std::uint16_t VARIANT = 0x8000;
        uuid make_uuid_v4(){
            std::ifstream urandom("/dev/urandom", std::ios::in|std::ios::binary);
            constexpr std::uint16_t UUID_VERSION = 0x4000;
            uuid tmp;
            urandom.read(reinterpret_cast<char*>(&tmp), sizeof(uuid));
            tmp.time_high_version &= TIME_HIGH_MAX;
            tmp.time_high_version |= UUID_VERSION;
            tmp.clock_seq_reserved &= CLOCK_SEQ_MAX;
            tmp.clock_seq_reserved |= VARIANT;
            return tmp;
        }
        uuid make_uuid_v7(){
            std::ifstream urandom("/dev/urandom", std::ios::in|std::ios::binary);
            constexpr std::uint16_t UUID_VERSION = 0x7000;
            constexpr std::uint64_t TIME_LOW_MASK = UINT32_MAX, TIME_MID_MASK = UINT16_MAX;
            uuid tmp;
            auto timepoint = std::chrono::system_clock::now().time_since_epoch();
            std::uint64_t ms_count = std::chrono::duration_cast<std::chrono::milliseconds>(timepoint).count();
            tmp.time_low &= (ms_count & TIME_LOW_MASK);
            tmp.time_mid &= ((ms_count>>32) & TIME_MID_MASK);
            char *start = reinterpret_cast<char*>(&tmp) + offsetof(uuid, time_high_version);
            urandom.read(start, sizeof(uuid) - offsetof(uuid, time_high_version));
            tmp.time_high_version &= TIME_HIGH_MAX;
            tmp.time_high_version |= UUID_VERSION;
            tmp.clock_seq_reserved &= CLOCK_SEQ_MAX;
            tmp.clock_seq_reserved |= VARIANT;
            return tmp;
        }
        int uuid_cmpnode(const uuid *lhs, const uuid *rhs){
            const char *left = reinterpret_cast<const char*>(lhs) + offsetof(uuid, node);
            const char *right = reinterpret_cast<const char*>(rhs) + offsetof(uuid, node);
            for(std::size_t i=0; i < sizeof(uuid)-offsetof(uuid, node); ++i)
                if(left[i] != right[i])
                    return -1;
            return 0;
        }
        bool operator==(const uuid& lhs, const uuid& rhs){
            const unsigned char *lhs_ = reinterpret_cast<const unsigned char*>(&lhs);
            const unsigned char *rhs_ = reinterpret_cast<const unsigned char*>(&rhs);
            for(std::size_t i=0; i < sizeof(uuid); ++i)
                if(lhs_[i] != rhs_[i]) return false;
            return true;
        }
        bool operator!=(const uuid& lhs, const uuid& rhs){
            return !(lhs == rhs);
        }
    }
}
