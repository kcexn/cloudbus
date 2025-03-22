/*     
*   Copyright 2024 Kevin Exton
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
#include <ios>
#include <streambuf>
#include <memory>
#include <array>
#include <vector>
#include <tuple>
#include <sys/types.h>
#include <sys/socket.h>

#pragma once
#ifndef IO_BUFFERS
#define IO_BUFFERS
namespace io{
    namespace buffers{         
        class pipebuf : public std::streambuf {
            public:
                using Base = std::streambuf;
                using traits = Base::traits_type;
                using int_type = Base::int_type;
                using char_type = Base::char_type;
                using buffer = std::vector<char>;
                using native_handle_type = int*;
                static constexpr std::size_t DEFAULT_BUFSIZE = 4096;
                
                
                pipebuf():
                pipebuf(std::ios_base::in | std::ios_base::out){}
                pipebuf(pipebuf&& other);
                explicit pipebuf(std::ios_base::openmode which);
                
                pipebuf& operator=(pipebuf&& other);
                
                native_handle_type native_handle() { return _pipe.data(); }
                void close_read(); 
                void close_write();
                std::size_t write_remaining();
                std::ios_base::openmode mode() { return _which; }
                
                ~pipebuf();
            protected:
            
                int sync() override; 
                std::streamsize showmanyc() override; 
                int_type underflow() override;
                
                int_type overflow(int_type ch = traits::eof()) override;
            private:
                std::ios_base::openmode _which{};
                buffer _read, _write;
                std::array<int, 2> _pipe{};
                std::size_t BUFSIZE;
                
                int _send(char_type *buf, std::size_t size);
                int _recv();
                void _mvrbuf();
                void _resizewbuf();
        };

        struct socket_message {
            using address_type = std::tuple<struct sockaddr_storage, socklen_t>;
            using ancillary_buffer = std::vector<char>;
            using data_buffer = std::tuple<struct iovec, std::vector<char> >;

            struct msghdr header;
            address_type addr;
            ancillary_buffer ancillary;
            data_buffer data;
        };
        class sockbuf : public std::streambuf {
            public:     
                using Base = std::streambuf;
                using pos_type = Base::pos_type;
                using off_type = Base::off_type;
                using int_type = Base::int_type;
                using traits_type = Base::traits_type;
                using char_type = Base::char_type;
                using size_type = std::size_t;
                using buffer_type = std::shared_ptr<socket_message>;
                using buffers_type = std::vector<buffer_type>;
                using native_handle_type = int;
                static constexpr size_type MIN_BUFSIZE = 65536;
                
                sockbuf();
                explicit sockbuf(native_handle_type sockfd, bool connected=false, std::ios_base::openmode which=(std::ios_base::in | std::ios_base::out));
                explicit sockbuf(int domain, int type, int protocol, std::ios_base::openmode which=(std::ios_base::in | std::ios_base::out));
                buffer_type connectto(const struct sockaddr *addr, socklen_t addrlen);

                const buffer_type& recvbuf() const { return _buffers.front(); }
                const buffer_type& sendbuf() const { return _buffers.back(); }
                native_handle_type& native_handle() { return _socket; }
                int err(){ return _errno; }
                
                ~sockbuf();

                sockbuf(const sockbuf& other) = delete;
                sockbuf& operator=(const sockbuf& other) = delete;
                sockbuf(sockbuf&& other) = delete;
                sockbuf& operator=(sockbuf&& other) = delete;

            protected:
                virtual pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) override;
                virtual pos_type seekpos(pos_type pos, std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) override;
                virtual int sync() override;
                virtual std::streamsize showmanyc() override;

                virtual std::streamsize xsputn(const char_type* s, std::streamsize count) override;
                virtual int_type overflow(int_type ch = traits_type::eof()) override;
                virtual int_type underflow() override;
                virtual std::streamsize xsgetn(char_type *s, std::streamsize count) override;
            private:
                std::ios_base::openmode _which;
                buffers_type _buffers;
                native_handle_type _socket;
                int _errno;
                bool _connected;
                
                void _init_buf_ptrs();
                int _send(const buffer_type& buf);
                void _resizewbuf(const buffer_type& buf);
                void _memmoverbuf();
                int _recv();
        };
    }
}
#endif
