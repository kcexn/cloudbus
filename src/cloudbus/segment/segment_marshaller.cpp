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
        namespace {
            template<class T>
            static bool shrink_to_fit(std::vector<T>& vec){
                static constexpr std::size_t THRESH = 256;
                bool resized = false;
                const std::size_t capacity = vec.capacity();
                if( capacity > THRESH &&
                    vec.size() < capacity/8
                ){
                    vec  = std::vector<T>(
                        std::make_move_iterator(vec.begin()),
                        std::make_move_iterator(vec.end())
                    );
                    resized = true;
                }
                return resized;
            }
        }
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
            auto begin = north().begin(), end = north().end();
            auto lb = std::lower_bound(
                begin,
                end,
                nsp,
                [](const auto& lhs, const stream_ptr& nsp) {
                    return lhs.ptr.owner_before(nsp);
                }
            );
            if(lb == end || !owner_equal(lb->ptr, nsp)) {
                auto put = std::remove_if(
                    begin,
                    lb,
                    [&](const auto& handle) {
                        return handle.ptr.expired();
                    }
                );
                if(put != lb) {
                    *put = marshaller::make_north(nsp);
                    lb = --north().erase(++put, lb);
                } else {
                    lb = north().insert(lb, marshaller::make_north(nsp));
                }
                const auto index = std::distance(north().begin(), lb);
                if(shrink_to_fit(north()))
                    lb = north().begin() + index;
            }
            xmsg_read(*lb->pbuf, *nsp);
            return lb;
        }
        marshaller::south_buffers::iterator marshaller::_marshal(const south_type::handle_type& stream){
            using stream_ptr = south_type::stream_ptr;
            const auto& ssp = std::get<stream_ptr>(stream);
            auto begin = south().begin(), end = south().end();
            auto lb = std::lower_bound(
                begin,
                end,
                ssp,
                [](const auto& lhs, const stream_ptr& ssp) {
                    return lhs.ptr.owner_before(ssp);
                }
            );
            if(lb == end || !owner_equal(lb->ptr, ssp)) {
                auto put = std::remove_if(
                    begin,
                    lb,
                    [&](const auto& handle) {
                        return handle.ptr.expired();
                    }
                );
                if(put != lb) {
                    *put = marshaller::make_south(ssp);
                    lb = --south().erase(++put, lb);
                } else {
                    lb = south().insert(lb, marshaller::make_south(ssp));
                }
                const auto index = std::distance(south().begin(), lb);
                if(shrink_to_fit(south()))
                    lb = south().begin() + index;
            }
            auto& buf = *lb->pbuf;
            if(buf.tellg() == buf.tellp()) {
                buf.seekg(0);
                stream_copy(buf.seekp(0), *ssp);
            }
            return lb;
        }
    }
}
