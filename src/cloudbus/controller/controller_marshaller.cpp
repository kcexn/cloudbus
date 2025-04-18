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
            auto it=north().begin(), cur=it, end=north().end();
            while((cur=it) != end){
                ++it;
                auto&[n, buf] = *cur;
                if(n.expired()) {
                    *cur = std::move(*(--end));
                    it = cur;
                } else if ( !(n.owner_before(nsp) || nsp.owner_before(n)) ) {
                    north().resize(end-north().begin());
                    if(buf.tellg() == buf.tellp()){
                        buf.seekg(0);
                        stream_copy(buf.seekp(0), *nsp);
                    }
                    return cur;
                }
            }
            north().resize(end-north().begin());
            auto&[ptr, buf] = north().emplace_back();
            ptr = nsp;
            stream_copy(buf, *nsp);
            return --north().end();
        }
        marshaller::south_buffers::iterator marshaller::_marshal(const south_type::handle_type& stream){
            using stream_ptr = south_type::stream_ptr;
            const auto& ssp = std::get<stream_ptr>(stream);
            auto it=south().begin(), cur=it, end=south().end();
            while((cur=it) != end){
                ++it;
                auto&[s, buf] = *cur;
                if(s.expired()) {
                    *cur = std::move(*(--end));
                    it=cur;
                } else if( !(s.owner_before(ssp) || ssp.owner_before(s)) ) {
                    south().resize(end-south().begin());
                    if(xmsg_read(buf, *ssp).bad())
                        return south().end();
                    return cur;
                }
            }
            south().resize(end-south().begin());
            auto&[ptr, buf] = south().emplace_back();
            ptr = ssp;
            xmsg_read(buf, *ssp);
            return --south().end();
        }
    }
}