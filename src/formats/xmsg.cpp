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
#include "xmsg.hpp"
#include <algorithm>
#include <cstring>
namespace cloudbus{
    namespace messages{
        
        static std::size_t count(const std::vector<xmsgbuf::buffer>& buffers, xmsgbuf::char_type *base, xmsgbuf::char_type *offset){
            std::size_t counter = 0;
            for(auto& buf: buffers){
                if(buf.data() == base) break;
                counter += std::tuple_size<xmsgbuf::buffer>();
            }
            return counter + (offset - base);
        }
        
        xmsgbuf::xmsgbuf(xmsgbuf&& other):
            Base(std::move(other)),
            _which{std::move(other._which)},
            _buffers{std::move(other._buffers)}
        {}
        xmsgbuf::xmsgbuf( std::ios_base::openmode which ):
            Base(),
            _which{which},
            _buffers{}
        {
            auto& buf = _buffers.emplace_back();
            Base::setp(buf.data(), buf.data() + buf.size());
            Base::setg(Base::pbase(), Base::pbase(), Base::pptr());
        }
        
        xmsgbuf& xmsgbuf::operator=(xmsgbuf&& other){
            _buffers = std::move(other._buffers);
            _which = std::move(other._which);
            Base::operator=(std::move(other));
            return *this;
        }
        
        uuid* xmsgbuf::eid(){
            auto& buf = _buffers[0];
            
            if(count(_buffers, Base::pbase(), Base::pptr()) >= sizeof(uuid)) 
                return reinterpret_cast<uuid*>(buf.data());
            return nullptr;
        }
        const uuid* xmsgbuf::eid() const {
            auto& buf = _buffers[0];
            
            if(count(_buffers, Base::pbase(), Base::pptr()) >= sizeof(uuid)) 
                return reinterpret_cast<const uuid*>(buf.data());
            return nullptr;
        }
        
        msglen* xmsgbuf::len(){ 
            constexpr size_type offset = offsetof(msgheader, len);
            auto& buf = _buffers[0];
            
            if(count(_buffers, Base::pbase(), Base::pptr()) >= offset + sizeof(msglen)) 
                return reinterpret_cast<msglen*>(&buf[offset]);
            return nullptr;
        }
        const msglen* xmsgbuf::len() const { 
            constexpr size_type offset = offsetof(msgheader, len);
            auto& buf = _buffers[0];
            
            if(count(_buffers, Base::pbase(), Base::pptr()) >= offset + sizeof(msglen)) 
                return reinterpret_cast<const msglen*>(&buf[offset]);
            return nullptr;
        }
        
        msgversion* xmsgbuf::version(){ 
            constexpr size_type offset = offsetof(msgheader, version);
            auto& buf = _buffers[0];
            
            if(count(_buffers, Base::pbase(), Base::pptr()) >= offset + sizeof(msgversion)) 
                return reinterpret_cast<msgversion*>(&buf[offset]);
            return nullptr;
        }
        const msgversion* xmsgbuf::version() const { 
            constexpr size_type offset = offsetof(msgheader, version);
            auto& buf = _buffers[0];
            
            if(count(_buffers, Base::pbase(), Base::pptr()) >= offset + sizeof(msgversion)) 
                return reinterpret_cast<const msgversion*>(&buf[offset]);
            return nullptr;
        }
        
        msgtype* xmsgbuf::type(){ 
            constexpr size_type offset = offsetof(msgheader, type);
            auto& buf = _buffers[0];
            
            if(count(_buffers, Base::pbase(), Base::pptr()) >= offset + sizeof(msgtype)) 
                return reinterpret_cast<msgtype*>(&buf[offset]);
            return nullptr;
        }
        
        const msgtype* xmsgbuf::type() const { 
            constexpr size_type offset = offsetof(msgheader, type);
            size_type pcount = count(_buffers, Base::pbase(), Base::pptr());
            auto& buf = _buffers[0];
            
            if(pcount >= offset + sizeof(msgtype)) 
                return reinterpret_cast<const msgtype*>(&buf[offset]);
            return nullptr;
        }

        bool xmsgbuf::complete() const {
            if(auto *_len = len(); _len != nullptr)
                return count(_buffers, Base::pbase(), Base::pptr()) == _len->length;
            return false;
        }

        std::streamsize xmsgbuf::xsputn(const char_type *s, std::streamsize count){
            for(std::size_t remaining = count, length = 0; remaining > 0; remaining -= length){
                if(Base::pptr() == Base::epptr()){
                    auto it = std::find_if(_buffers.begin(), _buffers.end(), [&](auto& b){
                        return b.data() == Base::pbase();
                    });
                    if(++it == _buffers.end()){
                        auto& buf = _buffers.emplace_back();
                        Base::setp(buf.data(), buf.data() + buf.size());
                    } else {
                        Base::setp(it->data(), it->data() + it->size());
                    }
                }
                length = std::min(remaining, static_cast<std::size_t>(Base::epptr() - Base::pptr()));
                std::memcpy(Base::pptr(), s + count - remaining, length);
                Base::pbump(length);
            }
            return count;
        }
        
        std::streamsize xmsgbuf::showmanyc(){
            size_type gc = count(_buffers, Base::eback(), Base::gptr());
            size_type pc = count(_buffers, Base::pbase(), Base::pptr());
            if(auto *len_ = len(); len_ != nullptr)
                if(gc == pc && len_->length == gc) return -1;
            return pc - gc;
        }
        xmsgbuf::int_type xmsgbuf::underflow(){
            if(Base::gptr() == Base::pptr()) return traits::eof();
            if(Base::eback() == Base::pbase()) {
                Base::setg(Base::eback(), Base::gptr(), Base::pptr());
                return traits::to_int_type(*Base::gptr());
            }
            auto it = std::find_if(_buffers.begin(), _buffers.end(), [&](auto& buf){ return buf.data() == Base::eback(); });
            if(++it != _buffers.end()){
                auto& buf = *it;
                if(buf.data() == Base::pbase()){
                    Base::setg(buf.data(), buf.data(), Base::pptr());
                } else {
                    Base::setg(buf.data(), buf.data(), buf.data() + buf.size());
                }
                return traits::to_int_type(*Base::gptr());
            }
            return traits::eof();
        }
        
        xmsgbuf::int_type xmsgbuf::overflow(int_type ch){
            auto it = std::find_if(_buffers.begin(), _buffers.end(), [&](auto& buf){ return buf.data() == Base::pbase(); });
            if(++it ==  _buffers.end()){
                auto& buf = _buffers.emplace_back();
                Base::setp(buf.data(), buf.data() + buf.size());
            } else {
                auto& buf = *it;
                Base::setp(buf.data(), buf.data() + buf.size());
            }
            if(traits::eq_int_type(ch, traits::eof())) return traits::eof();
            return Base::sputc(ch);
        }

        xmsgbuf::Base::pos_type xmsgbuf::seekoff(Base::off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which){
            Base::pos_type pos = 0;
            switch(dir){
                case std::ios_base::beg:
                    pos += off;
                    return seekpos(pos, which);
                case std::ios_base::end:
                    pos = count(_buffers, Base::pbase(), Base::pptr()) + off;
                    return seekpos(pos, which);
                case std::ios_base::cur:
                    if(which & std::ios_base::in){
                        pos = count(_buffers, Base::eback(), Base::gptr()) + off;
                    } else if (which & std::ios_base::out){
                        pos = count(_buffers, Base::pbase(), Base::pptr()) + off;
                    }
                    return seekpos(pos, which);
              default:
                    return Base::seekoff(off, dir, which);
            }
        }

        xmsgbuf::Base::pos_type xmsgbuf::seekpos(Base::pos_type pos, std::ios_base::openmode which){
            std::size_t pc = count(_buffers, Base::pbase(), Base::pptr());
            if(pos < 0) return Base::seekpos(pos, which);
            if(static_cast<std::size_t>(pos) > pc) return Base::seekpos(pos, which);
            if(auto n = pos/std::tuple_size<buffer>(); n < _buffers.size()){
                auto& buf = _buffers[n];
                auto off = pos % std::tuple_size<buffer>();
                if(which & std::ios_base::in){
                    if(buf.data() == Base::pbase())
                        Base::setg(buf.data(), buf.data() + off, Base::pptr());
                    else
                        Base::setg(buf.data(), buf.data() + off, buf.data() + std::tuple_size<buffer>());
                }
                if(which & std::ios_base::out){
                    Base::setp(buf.data(), buf.data() + std::tuple_size<buffer>());
                    Base::pbump(off);
                }
                return pos;
            } else return Base::seekpos(pos, which);
        }
        
        xmsgstream::xmsgstream(xmsgstream&& other):
            Base(&_buf),
            _buf(std::move(other._buf))
        {}        
        xmsgstream::xmsgstream( std::ios_base::openmode mode ):
            Base(&_buf),
            _buf(mode)
        {}
        xmsgstream& xmsgstream::operator=(xmsgstream&& other){
            _buf = std::move(other._buf);
            Base::operator=(std::move(other));
            return *this;
        }
    }
}
