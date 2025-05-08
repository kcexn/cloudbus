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
#include "controller_marshaller.hpp"
namespace cloudbus{
    namespace controller {
        static messages::xmsgstream& xmsg_read(messages::xmsgstream& buf, std::istream& is){
            constexpr std::streamsize HDRLEN=sizeof(messages::msgheader), BUFSIZE=256;
            std::array<char, BUFSIZE> _buf;
            std::streamsize gcount = 0, p;
            if(buf.eof()){
                buf.clear(buf.rdstate() & ~buf.eofbit);
                buf.seekp(0);
            }
            for(p = buf.tellp(); p < HDRLEN; p += gcount){
                if( (gcount = is.readsome(_buf.data(), HDRLEN-p)) ){
                    if(buf.write(_buf.data(), gcount).bad())
                        return buf;
                } else return buf;
            }
            for(std::streamsize rem = buf.len()->length-p; rem > 0; rem -= gcount){
                if( (gcount = is.readsome(_buf.data(), std::min(rem, BUFSIZE))) ){
                    if(buf.write(_buf.data(), gcount).bad())
                        return buf;
                } else return buf;
            }
            return buf;
        }
        static bool stream_copy(std::ostream& os, std::istream& is){
            std::array<char, 256> _buf;
            constexpr std::streamsize hdrlen = sizeof(messages::msgheader);
            std::streamsize maxlen = UINT16_MAX - hdrlen;
            while(auto gcount = is.readsome(_buf.data(), std::min(maxlen, static_cast<std::streamsize>(_buf.max_size())))){
                os.write(_buf.data(), gcount);
                maxlen -= gcount;
            }
            return is.eof();
        }

        marshaller::north_buffers::iterator marshaller::_unmarshal(const north_type::handle_type& stream){
            using stream_ptr = north_type::stream_ptr;
            const auto& nsp = std::get<stream_ptr>(stream);
            auto begin = north().begin(), end = north().end();
            auto it = begin, new_end = end;
            while(it != new_end){
                auto&[n, pbuf] = *it;
                if(n.expired()) {
                    *it = std::move(*--new_end);
                } else if (owner_equal(n, nsp)) {
                    north().erase(new_end, end);
                    if(auto& buf = *pbuf; buf.tellg()==buf.tellp()){
                        buf.seekg(0);
                        stream_copy(buf.seekp(0), *nsp);
                    }
                    return it;
                } else ++it;
            }
            north().erase(new_end, end);
            auto&[ptr, pbuf] = north().emplace_back(marshaller::make_north(nsp));
            stream_copy(*pbuf, *nsp);
            static constexpr std::size_t THRESH = 32;
            if( north().size() > THRESH &&
                north().size() < north().capacity()/8
            ){
                north() = north_buffers(
                    std::make_move_iterator(north().begin()),
                    std::make_move_iterator(north().end())
                );
            }
            return --north().end();
        }
        marshaller::south_buffers::iterator marshaller::_marshal(const south_type::handle_type& stream){
            using stream_ptr = south_type::stream_ptr;
            const auto& ssp = std::get<stream_ptr>(stream);
            auto begin = south().begin(), end = south().end();
            auto it = begin, new_end = end;
            while(it != new_end) {
                auto&[s, pbuf] = *it;
                if(s.expired()) {
                    *it = std::move(*--new_end);
                } else if(owner_equal(s, ssp)) {
                    south().erase(new_end, end);
                    if(xmsg_read(*pbuf, *ssp).bad())
                        return south().end();
                    return it;
                } else ++it;
            }
            south().erase(new_end, end);
            auto&[ptr, pbuf] = south().emplace_back(marshaller::make_south(ssp));
            xmsg_read(*pbuf, *ssp);
            static constexpr std::size_t THRESH = 32;
            if( south().size() > THRESH &&
                south().size() < south().capacity()/8
            ){
                south() = south_buffers(
                    std::make_move_iterator(south().begin()),
                    std::make_move_iterator(south().end())
                );
            }
            return --south().end();
        }
    }
}
