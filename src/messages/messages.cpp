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
#include <array>
#include <charconv>
#include <fstream>
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
            return std::memcmp(lhs->node, rhs->node, sizeof(lhs->node));
        }
        bool operator==(const uuid& lhs, const uuid& rhs){
            return !std::memcmp(&lhs, &rhs, sizeof(lhs));
        }
        bool operator!=(const uuid& lhs, const uuid& rhs){
            return !(lhs == rhs);
        }     
        std::ostream& operator<<(std::ostream& os, const uuid& gid){
            constexpr int HEX=16;
            const char *zeroes = "00";
            const std::uint8_t *first = reinterpret_cast<const std::uint8_t*>(&gid);
            const std::uint8_t *last = first + sizeof(gid);
            std::array<char, 2> buf = {0};
            std::size_t delim = 4;
            for(auto *cur=first; cur < last; ++cur){
                auto[ptr, ec] = std::to_chars(buf.data(), buf.data()+buf.max_size(), *cur, HEX);
                if(ec != std::errc()){
                    os.setstate(os.failbit);
                    return os;
                }
                auto len = ptr-buf.data();
                os.write(zeroes, buf.max_size()-len).write(buf.data(), len);
                if(cur-first == delim){
                    os.write("-", 1);
                    if(delim == offsetof(uuid, node))
                        delim += 6;
                    else delim += 2;
                }
            }
            return os;
        }
    }
}
