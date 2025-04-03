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
#include <cstring>
namespace cloudbus{
    namespace messages {
        static constexpr std::uint16_t VARIANT = 0x80;
        uuid make_uuid_v4(){
            constexpr std::uint16_t UUID_VERSION = 0x4000;
            if(std::ifstream urnd("/dev/urandom", urnd.in | urnd.binary);
                    urnd.good()
            ){
                uuid tmp;
                urnd.read(reinterpret_cast<char*>(&tmp), sizeof(uuid));
                tmp.time_high_version &= TIME_HIGH_MAX;
                tmp.time_high_version |= UUID_VERSION;
                tmp.clock_seq_reserved &= CLOCK_SEQ_MAX;
                tmp.clock_seq_reserved |= VARIANT;
                return tmp;
            } else return uuid{};
        }
        uuid make_uuid_v7(){
            constexpr std::uint16_t UUID_VERSION = 0x7000;
            constexpr std::uint64_t TIME_LOW_MASK = UINT32_MAX, TIME_MID_MASK = UINT16_MAX;
            if(std::ifstream urnd("/dev/urandom", urnd.in | urnd.binary);
                    urnd.good()
            ){
                uuid tmp;
                auto timepoint = std::chrono::system_clock::now().time_since_epoch();
                std::uint64_t ms_count = std::chrono::duration_cast<std::chrono::milliseconds>(
                                            timepoint).count();
                tmp.time_low = (ms_count & TIME_LOW_MASK);
                tmp.time_mid = ((ms_count>>32) & TIME_MID_MASK);
                char *start = reinterpret_cast<char*>(&tmp)+offsetof(uuid, time_high_version);
                urnd.read(start, sizeof(uuid)-offsetof(uuid, time_high_version));
                tmp.time_high_version &= TIME_HIGH_MAX;
                tmp.time_high_version |= UUID_VERSION;
                tmp.clock_seq_reserved &= CLOCK_SEQ_MAX;
                tmp.clock_seq_reserved |= VARIANT;
                return tmp;
            } else return uuid{};
        }
        int uuidcmp_node(const uuid *lhs, const uuid *rhs){
            auto *left = reinterpret_cast<const char*>(lhs) + offsetof(uuid, node);
            auto *right = reinterpret_cast<const char*>(rhs) + offsetof(uuid, node);
            return std::memcmp(left, right, sizeof(uuid)-offsetof(uuid, node));
        }
        int uuidcmp(const uuid *lhs, const uuid *rhs){
            return std::memcmp(lhs, rhs, sizeof(uuid));
        }
        bool operator==(const uuid& lhs, const uuid& rhs){
            return !uuidcmp(&lhs, &rhs);
        }
        bool operator!=(const uuid& lhs, const uuid& rhs){
            return !(lhs == rhs);
        }
    }
}
