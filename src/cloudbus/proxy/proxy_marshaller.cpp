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
#include "proxy_marshaller.hpp"
namespace cloudbus{
    namespace proxy {
        static bool xmsg_read(messages::xmsgstream& buf, std::istream& is){
            std::array<char, 256> _buf;
            constexpr std::streamsize HDRLEN = sizeof(messages::msgheader);
            std::streamsize gcount = 0, p = 0;
            if(buf.eof()){
                buf.clear(buf.rdstate() & ~buf.eofbit);
                buf.seekp(0);
            }
            for(p = buf.tellp(); p < HDRLEN; p += gcount){
                if((gcount = is.readsome(_buf.data(), HDRLEN - p))){
                    if(buf.write(_buf.data(), gcount).bad())
                        throw std::runtime_error("Unable to write to xmsg buffer.");
                } else return is.eof();
            }
            for(std::uint16_t rem = buf.len()->length - p; rem > 0; rem -= gcount){
                if((gcount = is.readsome(_buf.data(), std::min(rem, static_cast<std::uint16_t>(_buf.max_size()))))){
                    if(buf.write(_buf.data(), gcount).bad())
                        throw std::runtime_error("Unable to write to xmsg buffer.");
                } else return is.eof();
            }
            return is.eof();
        }

        marshaller::north_buffers::iterator marshaller::_unmarshal(const north_type::handle_type& stream){
            auto& nsp = std::get<north_type::stream_ptr>(stream);
            for(auto it = north().begin(); it < north().end(); ++it){
                auto&[n, buf] = *it;
                if(!n.owner_before(nsp)){
                    xmsg_read(buf, *nsp);
                    return it;
                }
                if(n.expired())
                    it = --north().erase(it);
            }
            auto&[ptr, buf] = north().emplace_back();
            ptr = nsp;
            xmsg_read(buf, *nsp);
            return --north().end();
        }
        marshaller::south_buffers::iterator marshaller::_marshal(const south_type::handle_type& stream){
            auto& ssp = std::get<south_type::stream_ptr>(stream);
            for(auto it = south().begin(); it < south().end(); ++it){
                auto&[s, buf] = *it;
                if(!s.owner_before(ssp)){
                    xmsg_read(buf, *ssp);
                    return it;
                }
                if(s.expired())
                    it = --south().erase(it);
            }
            auto&[ptr, buf] = south().emplace_back();
            ptr = ssp;
            xmsg_read(buf, *ssp);
            return --south().end();
        }
    }
}