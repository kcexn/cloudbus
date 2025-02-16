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
        using sockopt = std::tuple<std::string, std::vector<char> >;
            
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
                using int_type = Base::int_type;
                using traits_t = Base::traits_type;
                using char_type = Base::char_type;
                using buffer = std::vector<char>;
                using size_type = std::size_t;
                using native_handle_type = int;
                using msghdr_t = struct msghdr;
                using iovec = struct iovec;
                using msghdr_array_t = std::array<msghdr_t, 2>;
                using cbuf_array_t = std::array<buffer, 2>;
                using address_type = std::tuple<struct sockaddr_storage, socklen_t>;
                using storage_array = std::array<address_type, 2>;
                static constexpr size_type DEFAULT_BUFSIZE = 16535;
                
                
                sockbuf();
                sockbuf(int domain, int type, int protocol)
                    :   sockbuf(domain, type, protocol, {}, std::ios_base::in | std::ios_base::out){}
                    
                sockbuf(int domain, int type, int protocol, std::initializer_list<sockopt> l):
                sockbuf(domain, type, protocol, l, std::ios_base::in | std::ios_base::out){}
                    
                sockbuf(native_handle_type sockfd):
                sockbuf(sockfd, std::ios_base::in | std::ios_base::out){}
                    
                sockbuf(sockbuf&& other);
                explicit sockbuf(native_handle_type sockfd, std::ios_base::openmode which);
                explicit sockbuf(int domain, int type, int protocol, std::initializer_list<sockopt> l, std::ios_base::openmode which);

                sockbuf& operator=(sockbuf&& other);
                
                size_type& bufsize(){ return BUFSIZE; }
                cbuf_array_t& cmsgs() { return _cbufs; }
                msghdr_array_t& msghdrs() { return _msghdrs; }
                storage_array& addresses() { return _addresses; }

                int err(){ return _errno; }
                
                void pubsetopt(sockopt opt){ return setopt(opt); }
                optval pubgetopt(sockopt opt){ return getopt(opt); }

                int connectto(const struct sockaddr* addr, socklen_t addrlen);
                
                native_handle_type native_handle() { return _socket; }
                ~sockbuf();
            protected:
                Base::pos_type seekoff(Base::off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) override;
                Base::pos_type seekpos(Base::pos_type pos, std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) override;
                int sync() override;
                std::streamsize showmanyc() override;
                int_type overflow(int_type ch = traits_t::eof()) override;
                int_type underflow() override;
                
                virtual void setopt(sockopt opt);
                virtual optval getopt(sockopt opt);
            private:
                size_type BUFSIZE;
                std::ios_base::openmode _which{};
                std::vector<buffer> _buffers{};
                cbuf_array_t _cbufs{};
                msghdr_array_t _msghdrs{};
                storage_array _addresses{};
                native_handle_type _socket{};
                std::array<iovec, 2> _iov{};
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
