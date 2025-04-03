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
#include <cstdint>
#pragma once
#ifndef CLOUDBUS_MESSAGE_TYPES
#define CLOUDBUS_MESSAGE_TYPES
namespace cloudbus{
    namespace messages{
        typedef struct {
            std::uint32_t time_low;
            std::uint16_t time_mid;
            std::uint16_t time_high_version;
            std::uint8_t clock_seq_reserved;
            std::uint8_t clock_seq_low;
            char node[6];
        } uuid;
        // UUID utils.
        constexpr std::uint16_t TIME_HIGH_MAX = 0x0FFF;
        constexpr std::uint16_t CLOCK_SEQ_MAX = 0x3F;
        uuid make_uuid_v4();
        uuid make_uuid_v7();
        int uuidcmp_node(const uuid *lhs, const uuid *rhs);
        bool operator==(const uuid& lhs, const uuid& rhs);
        bool operator!=(const uuid& lhs, const uuid& rhs);

        // Message structures.
        typedef struct {
            std::uint16_t seqno;
            std::uint16_t length; // number of bytes in this envelope.
        } msglen; // 4 bytes.
        typedef struct {
            std::uint8_t major;
            std::uint8_t minor;
        } msgversion;
        enum opcodes : std::uint8_t {
            DATA,
            STOP
        };
        enum session_flags : std::uint8_t {
            INIT = 1 << 7,
            ABORT = 1 << 6
        };
        typedef struct {
            std::uint8_t op; // 8 bit msg op code.
            std::uint8_t flags; // 8 bits for msg op code flags.
        } msgtype;
        // Message Envelope.
        typedef struct {
            uuid eid;               // 16 bytes 16 bytes -- Envelope ID
            msglen len;             // 4 bytes  20 bytes -- Length of message, including the envelope.
            msgversion version;     // 2 bytes  22 bytes
            msgtype type;           // 2 bytes  24 bytes.
        } msgheader;
    }
}
#endif
