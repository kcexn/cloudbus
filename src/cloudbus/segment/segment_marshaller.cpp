/*
*   Copyright 2025 Kevin Exton
*   This file is part of Cloudbus.
*
* Cloudbus is free software: you can redistribute it and/or modify it under the
*   terms of the GNU Affero General Public License as published by the Free Software
*   Foundation, either version 3 of the License, or any later version.
*
* Cloudbus is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
*   without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*   See the GNU Affero General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License along with Cloudbus.
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
            auto& nsp = std::get<stream_ptr>(stream);
            for(auto it = north().begin(); it < north().end(); ++it){
                auto&[n, buf] = *it;
                if(n.expired()) {
                    it = --north().erase(it);
                } else if (n.lock() == nsp) {
                    if(xmsg_read(buf, *nsp).bad())
                        return north().end();
                    return it;
                }
            }
            auto&[ptr, buf] = north().emplace_back();
            ptr = nsp;
            xmsg_read(buf, *nsp);
            return --north().end();
        }
        marshaller::south_buffers::iterator marshaller::_marshal(const south_type::handle_type& stream){
            using stream_ptr = south_type::stream_ptr;
            auto& ssp = std::get<stream_ptr>(stream);
            for(auto it = south().begin(); it < south().end(); ++it){
                auto&[s, buf] = *it;
                if(s.expired()) {
                    it = --south().erase(it);
                } else if(s.lock() == ssp) {
                    if(buf.tellg() == buf.tellp()){
                        buf.seekg(0);
                        stream_copy(buf.seekp(0), *ssp);
                    }
                    return it;
                }
            }
            auto&[ptr, buf] = south().emplace_back();
            ptr = ssp;
            stream_copy(buf, *ssp);
            return --south().end();
        }
    }
}