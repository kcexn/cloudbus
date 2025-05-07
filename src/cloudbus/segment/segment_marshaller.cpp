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
#include "segment_marshaller.hpp"
namespace cloudbus{
    namespace segment {
        static messages::xmsgstream& xmsg_read(messages::xmsgstream& buf, std::istream& is){
            constexpr std::streamsize HDRLEN = sizeof(messages::msgheader), BUFSIZE=256;
            std::array<char, BUFSIZE> _buf;
            std::streamsize gcount=0, p;
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
        static std::ostream& stream_copy(std::ostream& os, std::istream& is){
            constexpr std::streamsize HDRLEN = sizeof(messages::msgheader), BUFLEN=256;
            std::streamsize maxlen = UINT16_MAX - HDRLEN;
            std::array<char, BUFLEN> buf;
            while(auto gcount = is.readsome(buf.data(), std::min(maxlen, BUFLEN))){
                if(os.write(buf.data(), gcount).bad())
                    return os;
                if(!(maxlen-=gcount))
                    return os;
            }
            return os;
        }

        marshaller::north_buffers::iterator marshaller::_unmarshal(const north_type::handle_type& stream){
            using stream_ptr = north_type::stream_ptr;
            const auto& nsp = std::get<stream_ptr>(stream);
            auto it = north().begin(), end = north().end();
            while(it != end){
                auto&[n, pbuf] = *it;
                if(n.expired()) {
                    *it = std::move(*--end);
                } else if (owner_equal(n, nsp)) {
                    north().erase(end, north().end());
                    if(xmsg_read(*pbuf, *nsp).bad())
                        return north().end();
                    return it;
                } else ++it;
            }
            north().erase(end, north().end());
            auto&[ptr, pbuf] = north().emplace_back(marshaller::make_north(nsp));
            xmsg_read(*pbuf, *nsp);
            if(north().size() < north().capacity()/8)
                north().shrink_to_fit();
            return --north().end();
        }
        marshaller::south_buffers::iterator marshaller::_marshal(const south_type::handle_type& stream){
            using stream_ptr = south_type::stream_ptr;
            const auto& ssp = std::get<stream_ptr>(stream);
            auto it = south().begin(), end=south().end();
            while(it != end){
                auto&[s, pbuf] = *it;
                if(s.expired()) {
                    *it = std::move(*--end);
                } else if(owner_equal(s, ssp)) {
                    south().erase(end, south().end());
                    if(auto& buf=*pbuf; buf.tellg()==buf.tellp()){
                        buf.seekg(0);
                        stream_copy(buf.seekp(0), *ssp);
                    }
                    return it;
                } else ++it;
            }
            south().erase(end, south().end());
            auto&[ptr, pbuf] = south().emplace_back(marshaller::make_south(ssp));
            stream_copy(*pbuf, *ssp);
            if(south().size() < south().capacity()/8)
                south().shrink_to_fit();
            return --south().end();
        }
    }
}
