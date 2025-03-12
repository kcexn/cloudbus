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
#include "proxy_marshallers.hpp"
namespace cloudbus{
    namespace proxy {
        static std::array<char, 256> _buf = {};
        static bool xmsg_read(messages::xmsgstream& buf, std::istream& is){
            constexpr std::streamsize HDRLEN = sizeof(messages::msgheader);
            std::streamsize gcount = 0, p = 0;
            if(buf.eof()){
                buf.seekg(0);
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

        proxy_marshaller::north_buffers::iterator proxy_marshaller::_unmarshal(const north_type::handle_ptr& stream){
            for(auto it = north().begin(); it < north().end();){
                if(auto n = std::get<north_ptr>(*it).lock()){
                    if(n == std::get<north_type::stream_ptr>(*stream)){
                        auto& buf = std::get<north_format>(*it);
                        xmsg_read(buf, *n);
                        return it;
                    }
                    ++it;
                } else it = north().erase(it);
            }
            std::get<north_ptr>(north().emplace_back()) = std::get<north_type::stream_ptr>(*stream);
            auto n = std::get<north_ptr>(north().back()).lock();
            auto& buf = std::get<north_format>(north().back());
            xmsg_read(buf, *n);
            return --north().end(); 
        }
        proxy_marshaller::south_buffers::iterator proxy_marshaller::_marshal(const south_type::handle_ptr& stream){
            for(auto it = south().begin(); it < south().end();){
                if(auto s = std::get<south_ptr>(*it).lock()){
                    if(s == std::get<south_type::stream_ptr>(*stream)){
                        auto& buf = std::get<south_format>(*it);
                        xmsg_read(buf, *s);
                        return it;
                    }
                    ++it;
                } else it = south().erase(it);
            }
            std::get<south_ptr>(south().emplace_back()) = std::get<south_type::stream_ptr>(*stream);
            auto s = std::get<south_ptr>(south().back()).lock();
            auto& buf = std::get<south_format>(south().back());
            xmsg_read(buf, *s);
            return --south().end();
        }
    }
}