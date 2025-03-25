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
#pragma once
#ifndef CLOUDBUS_XMSG
#define CLOUDBUS_XMSG
namespace cloudbus{
    namespace messages {
        class xmsgbuf : public std::streambuf {
            public:
                static constexpr std::size_t BUFINC = 4*1024;
                using Base = std::streambuf;
                
                xmsgbuf();
                xmsgbuf(xmsgbuf&& other) noexcept;
                xmsgbuf& operator=(xmsgbuf&& other) noexcept;

                void swap(xmsgbuf& other) noexcept;
                                
                uuid *eid() noexcept;
                msglen *len() noexcept;
                msgversion *version() noexcept;
                msgtype *type() noexcept;
                
                ~xmsgbuf();

                xmsgbuf(const xmsgbuf& other) = delete;
                xmsgbuf& operator=(const xmsgbuf& other) = delete;
                
            protected:
                virtual std::streamsize xsputn(const char *s, std::streamsize count) override;
                virtual std::streamsize showmanyc() override;
                virtual int_type underflow() override;
                virtual int_type overflow(int_type ch) override;
                virtual pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which) override;
                virtual pos_type seekpos(pos_type pos, std::ios_base::openmode which) override;
                
            private:
                void *bufptr;
                std::size_t bufsize;
        };
        
        class xmsgstream : public std::iostream {
            public:
                using Base = std::iostream;
                
                xmsgstream();
                xmsgstream(xmsgstream&& other) noexcept;               
                xmsgstream& operator=(xmsgstream&& other) noexcept;

                void swap(xmsgstream& other) noexcept;

                uuid* eid() noexcept { return _buf.eid(); }
                msglen* len() noexcept { return _buf.len(); }                
                msgversion* version() noexcept { return _buf.version(); }                
                msgtype* type() noexcept { return _buf.type(); }
                
                ~xmsgstream() = default;

                xmsgstream(const xmsgstream& other) = delete;
                xmsgstream& operator=(const xmsgstream& other) = delete;
            private:
                xmsgbuf _buf;
        };
    }
}
#endif