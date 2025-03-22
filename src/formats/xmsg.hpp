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
#include "../messages.hpp"
#include <streambuf>
#include <iostream>
#include <array>
#include <vector>
#pragma once
#ifndef CLOUDBUS_XMSG
#define CLOUDBUS_XMSG
namespace cloudbus{
    namespace messages {
        class xmsgbuf : public std::streambuf {
            public:
                static constexpr std::size_t bufsize = 16384; // 16kB
                using Base = std::streambuf;
                using int_type = Base::int_type;
                using char_type = Base::char_type;
                using traits = Base::traits_type;
                using buffer = std::array<char, bufsize>;
                using size_type = std::size_t;
                
                xmsgbuf()
                    : xmsgbuf( std::ios_base::in | std::ios_base::out ) {}                
                xmsgbuf(xmsgbuf&& other);
                explicit xmsgbuf( std::ios_base::openmode which );
                
                xmsgbuf& operator=(xmsgbuf&& other);
                
                uuid *eid();
                const uuid *eid() const;
                
                msglen *len();
                const msglen *len() const;
                
                msgversion *version();
                const msgversion *version() const;
                
                msgtype *type();
                const msgtype *type() const;

                bool complete() const;
                
                ~xmsgbuf() = default;
                
            protected:
                std::streamsize xsputn(const char_type *s, std::streamsize count) override;
                std::streamsize showmanyc() override;
                int_type underflow() override;  
                int_type overflow(int_type ch = traits::eof()) override;
                Base::pos_type seekoff( Base::off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which = std::ios_base::in | std::ios_base::out ) override;
                Base::pos_type seekpos( Base::pos_type pos, std::ios_base::openmode which = std::ios_base::in | std::ios_base::out ) override;
                
            private:
                std::ios_base::openmode _which;
                std::vector<buffer> _buffers;
        };
        
        class xmsgstream : public std::iostream {
            public:
                using Base = std::iostream;
                using size_type = xmsgbuf::size_type;
                using char_type = xmsgbuf::char_type;
                
                xmsgstream()
                    : xmsgstream(std::ios_base::in | std::ios_base::out) {}
                xmsgstream(xmsgstream&& other);
                explicit xmsgstream( std::ios_base::openmode mode );
                
                xmsgstream& operator=(xmsgstream&& other);
                                
                uuid* eid(){ return _buf.eid(); }
                const uuid* eid() const { return _buf.eid(); }

                msglen* len() { return _buf.len(); }
                const msglen* len() const { return _buf.len(); }
                
                msgversion* version() { return _buf.version(); }
                const msgversion* version() const { return _buf.version(); }
                
                msgtype* type() { return _buf.type(); }
                const msgtype* type() const { return _buf.type(); }
                
                bool complete() const { return _buf.complete(); }
                
                ~xmsgstream() = default;
            private:
                xmsgbuf _buf;
        };
    }
}
#endif