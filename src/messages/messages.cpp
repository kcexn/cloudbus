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
            uuid tmp{};
            if(std::ifstream urnd("/dev/urandom", urnd.in | urnd.binary);
                urnd.good()
            ){
                urnd.read(reinterpret_cast<char*>(&tmp), sizeof(uuid));
                tmp.time_high_version &= TIME_HIGH_MAX;
                tmp.time_high_version |= UUID_VERSION;
                tmp.clock_seq_reserved &= CLOCK_SEQ_MAX;
                tmp.clock_seq_reserved |= VARIANT;
                return tmp;
            }
            return tmp;
        }
        uuid make_uuid_v7(){
            constexpr std::uint16_t UUID_VERSION = 0x7000;
            constexpr std::uint64_t TIME_LOW_MASK = UINT32_MAX, TIME_MID_MASK = UINT16_MAX;
            uuid tmp{};
            if(std::ifstream urnd("/dev/urandom", urnd.in | urnd.binary);
                urnd.good()
            ){
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
            }
            return tmp;
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
            constexpr std::size_t NODE_OFF=offsetof(uuid,node);
            constexpr std::size_t STRSIZE=2*sizeof(gid)+4;
            const char *zeroes = "00";
            const std::uint8_t *first = reinterpret_cast<const std::uint8_t*>(&gid);
            const std::uint8_t *last = first + sizeof(gid);
            std::array<char, 2> buf = {};
            std::string id;
            id.reserve(STRSIZE);
            std::size_t delim = 4;
            for(auto *cur=first; cur < last; ++cur){
                if(cur-first == delim){
                    id.push_back('-');
                    delim += (delim==NODE_OFF) ? 6 : 2;
                }
                auto[ptr, ec] = std::to_chars(
                    buf.data(),
                    buf.data() + buf.max_size(),
                    *cur, HEX
                );
                if(ec != std::errc()){
                    os.setstate(os.failbit);
                    return os;
                }
                auto len = ptr-buf.data();
                id.append(zeroes, buf.max_size()-len);
                id.append(buf.data(), len);
            }
            return os.write(id.c_str(), id.size());
        }
    }
}
