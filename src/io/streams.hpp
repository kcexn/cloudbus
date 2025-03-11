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
#include "buffers.hpp"
#include <iostream>
#include <ios>
#include <initializer_list>

#pragma once
#ifndef IO_STREAMS
#define IO_STREAMS
namespace io{
    namespace streams{
        using optname = std::string;
        using optval = std::vector<char>;
        using sockopt = std::tuple<std::string, std::vector<char> >;
        
        class pipestream: public std::iostream {
            using Base = std::iostream;
            using native_handle_type = int*;
            buffers::pipebuf _buf;
            public:
                pipestream():
                    pipestream(std::ios_base::in | std::ios_base::out){}
                    
                pipestream(pipestream&& other):
                    Base(&_buf),
                    _buf(std::move(other._buf))
                {}
                
                explicit pipestream(std::ios_base::openmode which):
                    Base(&_buf),
                    _buf(which)
                {}
                
                native_handle_type native_handle() { return _buf.native_handle(); }
                void close_read() { return _buf.close_read(); }
                void close_write() { return _buf.close_write(); }
                std::size_t write_remaining() { return _buf.write_remaining(); }
                
                ~pipestream(){}
        };
        
        class sockstream: public std::iostream {
            using Base = std::iostream;
            using sockbuf = buffers::sockbuf;
            sockbuf _buf;
            
            public:
                using native_handle_type = buffers::sockbuf::native_handle_type;
                using optval = std::vector<char>;
            
                sockstream()
                    : Base(&_buf){}
                sockstream(int domain, int type, int protocol):
                    sockstream(domain, type, protocol, std::ios_base::in | std::ios_base::out){}
                sockstream(native_handle_type sockfd):
                    Base(&_buf), _buf(sockfd) {}
                sockstream(native_handle_type sockfd, std::ios_base::openmode which):
                    Base(&_buf), _buf(sockfd, which) {}
                sockstream(int domain, int type, int protocol, std::ios_base::openmode which):
                    Base(&_buf), _buf(domain, type, protocol, which) {}

                sockbuf::native_handle_type native_handle() { return _buf.native_handle(); }
                int err() { return _buf.err(); }
                sockbuf::buffer_type connectto(const struct sockaddr* addr, socklen_t len) { return _buf.connectto(addr, len); }
                ~sockstream(){}
        };
    }
}
#endif
