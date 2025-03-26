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
        xmsgbuf::xmsgbuf():
            Base(),
            bufptr{nullptr}, bufsize{0}
        {
            if( !(bufptr=std::malloc(BUFINC)) )
                throw std::bad_alloc();
            bufsize = BUFINC;
            char *base = static_cast<char*>(bufptr);
            setp(base, base+BUFINC);
            setg(pbase(), pbase(), pptr());
        }

        xmsgbuf::xmsgbuf(xmsgbuf&& other) noexcept:
            Base(), bufptr{nullptr}, bufsize{0}
        { swap(other); }

        xmsgbuf& xmsgbuf::operator=(xmsgbuf&& other) noexcept{
            swap(other);
            return *this;
        }
        void xmsgbuf::swap(xmsgbuf& other) noexcept{
            using std::swap;
            swap(bufptr, other.bufptr);
            swap(bufsize, other.bufsize);
            Base::swap(other);
        }
        uuid *xmsgbuf::eid() noexcept {
            std::size_t len = pptr()-pbase();
            if(len >= sizeof(uuid))
                return static_cast<uuid*>(bufptr);
            return nullptr;
        }
        msglen *xmsgbuf::len() noexcept { 
            constexpr std::size_t OFF = offsetof(msgheader, len);
            char *base = static_cast<char*>(bufptr);
            std::size_t len = pptr()-pbase();
            if(len >= OFF + sizeof(msglen))
                return reinterpret_cast<msglen*>(base+OFF);
            return nullptr;
        }
        msgversion *xmsgbuf::version() noexcept{ 
            constexpr std::size_t OFF = offsetof(msgheader, version);
            char *base = static_cast<char*>(bufptr);
            std::size_t len = pptr()-pbase();
            if(len >= OFF + sizeof(msgversion))
                return reinterpret_cast<msgversion*>(base+OFF);
            return nullptr;
        }
        msgtype *xmsgbuf::type() noexcept{
            constexpr std::size_t OFF = offsetof(msgheader, type);
            char *base = static_cast<char*>(bufptr);
            std::size_t len = pptr()-pbase();
            if(len >= OFF + sizeof(msgtype))
                return reinterpret_cast<msgtype*>(base+OFF);
            return nullptr;
        }
        std::streamsize xmsgbuf::xsputn(const char *s, std::streamsize count){
            std::streamsize len=0, n=0;
            while((s+=n) && (len+=n) < count){
                while( !(n = std::min(epptr()-pptr(), count-len)) ){
                    if(traits_type::eq_int_type(overflow(*s), traits_type::eof()))
                        return len;
                    ++len; ++s;
                }
                std::memcpy(pptr(), s, n);
                pbump(n);
            }
            return len;
        }
        std::streamsize xmsgbuf::showmanyc(){
            if(auto *lenp = len();
                pptr()==gptr() &&
                lenp->length == gptr()-eback())
            { 
                return -1; 
            }
            return pptr()-gptr();
        }
        xmsgbuf::int_type xmsgbuf::underflow(){
            if(showmanyc() < 0)
                return traits_type::eof();
            setg(eback(), gptr(), pptr());
            return traits_type::to_int_type(*gptr());
        }
        xmsgbuf::int_type xmsgbuf::overflow(int_type ch){
            auto poff = pptr()-pbase(), goff = gptr()-eback();
            if(auto *lp = len(); lp && lp->length == pptr()-pbase())
                return traits_type::eof();
            bufsize += BUFINC;
            if( !(bufptr=std::realloc(bufptr, bufsize)) )
                return traits_type::eof();
            char *base = static_cast<char*>(bufptr);
            setp(base, base+bufsize);
            pbump(poff);
            setg(pbase(), pbase()+goff, pptr());
            return sputc(ch);
        }
        xmsgbuf::pos_type xmsgbuf::seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which){
            pos_type pos=0;
            switch(dir){
                case std::ios_base::beg:
                    return seekpos(pos+off, which);
                case std::ios_base::end:
                    if(which & std::ios_base::in){
                        pos = egptr()-eback();
                    } else if (which & std::ios_base::out) {
                        pos = epptr()-pbase();
                    }
                    return seekpos(pos+off, which);
                case std::ios_base::cur:
                    if(which & std::ios_base::in){
                        pos = gptr()-eback();
                    } else if (which & std::ios_base::out){
                        pos = pptr()-pbase();
                    }
                    return seekpos(pos+off, which);
              default:
                    return Base::seekoff(off, dir, which);
            }
        }
        xmsgbuf::pos_type xmsgbuf::seekpos(pos_type pos, std::ios_base::openmode which){
            if(pos < 0) 
                return Base::seekpos(pos, which);
            if(which & std::ios_base::in){
                if(pos > pptr()-pbase())
                    return Base::seekpos(pos, which);
                setg(eback(), eback()+pos, pptr());
            } else if (which & std::ios_base::out){
                if(pos > epptr()-pbase())
                    return Base::seekpos(pos, which);
                setp(pbase(), epptr());
                pbump(pos);
                setg(eback(), std::min(gptr(), pptr()), pptr());
            }
            return pos;
        }
        xmsgbuf::~xmsgbuf(){
            if(bufptr)
                std::free(bufptr);
        }
        
        xmsgstream::xmsgstream():
            Base(&_buf), _buf{}
        {}

        xmsgstream::xmsgstream(xmsgstream&& other) noexcept:
            xmsgstream()
        { swap(other); }

        xmsgstream& xmsgstream::operator=(xmsgstream&& other) noexcept
        {
            swap(other);
            return *this;
        }

        void xmsgstream::swap(xmsgstream& other) noexcept
        {
            _buf.swap(other._buf);
            Base::swap(other);
        }
    }
}
