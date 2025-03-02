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
#include <initializer_list>
#include <streambuf>
#include <array>
#include <vector>
#include <string>
#include <tuple>
#include <sys/types.h>
#include <sys/socket.h>

#pragma once
#ifndef IO_BUFFERS
#define IO_BUFFERS
namespace io{
    namespace buffers{
        using optname = std::string;
        using optval = std::vector<char>;           
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
        
        
        
        class sockbuf : public std::streambuf {
            public:     
                using Base = std::streambuf;
                using pos_type = Base::pos_type;
                using off_type = Base::off_type;
                using int_type = Base::int_type;
                using traits_type = Base::traits_type;
                using char_type = Base::char_type;
                using buffer_type = std::vector<char>;
                using buffers_type = std::array<buffer_type, 2>;
                using size_type = std::size_t;
                using native_handle_type = int;
                using cbuf_array_t = std::array<buffer_type, 2>;
                using address_type = std::tuple<struct sockaddr_storage, socklen_t>;
                using storage_array = std::array<address_type, 2>;
                static constexpr size_type MIN_BUFSIZE = 16384;
                
                sockbuf();
                sockbuf(int domain, int type, int protocol):   
                    sockbuf(domain, type, protocol, std::ios_base::in | std::ios_base::out){}
                sockbuf(native_handle_type sockfd):
                    sockbuf(sockfd, std::ios_base::in | std::ios_base::out){}
                    
                sockbuf(sockbuf&& other);
                explicit sockbuf(native_handle_type sockfd, std::ios_base::openmode which);
                explicit sockbuf(int domain, int type, int protocol, std::ios_base::openmode which);

                sockbuf& operator=(sockbuf&& other);
                
                cbuf_array_t& cmsgs() { return _cbufs; }
                storage_array& addresses() { return _addresses; }

                int err(){ return _errno; }
                int connectto(const struct sockaddr* addr, socklen_t addrlen);
                
                native_handle_type native_handle() { return _socket; }
                ~sockbuf();
            protected:
                virtual pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) override;
                virtual pos_type seekpos(pos_type pos, std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) override;
                virtual int sync() override;
                virtual std::streamsize showmanyc() override;

                virtual std::streamsize xsputn(const char *s, std::streamsize count) override;
                virtual int_type overflow(int_type ch = traits_type::eof()) override;
                virtual int_type underflow() override;
            private:
                std::ios_base::openmode _which;
                buffers_type _buffers;
                cbuf_array_t _cbufs;
                storage_array _addresses;
                native_handle_type _socket;
                int _errno;
                bool _connected;
                
                void _init_buf_ptrs();
                int _send(char_type *buf, size_type size);
                int _recv();
                void _memmoverbuf();
                void _resizewbuf();
        };
    }
}
#endif
