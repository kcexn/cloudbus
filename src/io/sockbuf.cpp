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
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <cctype>
#include <cstring>
#include <cstdint>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
namespace io{
    namespace buffers{
        static int _poll(int socket, short events){
            struct pollfd fd = {socket, events};
            while(poll(&fd, 1, -1) < 0){
                switch(errno){
                    case EINTR: continue;
                    default: return -1;
                }
            }
            if(fd.revents & (POLLHUP | POLLERR | POLLNVAL))
                return -1;
            return 0;
        }
        void sockbuf::_init_buf_ptrs(){
            for(std::size_t i=0; i < 2; ++i)
                _buffers.push_back(std::make_shared<socket_message>());
            if(_which & std::ios_base::in){
                auto& buf = _buffers[0];
                auto&[from, len] = buf->addr;
                len = sizeof(from);
                std::memset(&from, 0, sizeof(from));
                auto& recvbuf = buf->data;
                if( !(recvbuf.iov_base = std::malloc(MIN_BUFSIZE)) )
                    throw std::runtime_error("Unable to allocate space in receive buffer.");
                recvbuf.iov_len = MIN_BUFSIZE;
                char *data = reinterpret_cast<char*>(recvbuf.iov_base);
                setg(data, data, data);
            }
            if(_which & std::ios_base::out){
                auto& buf = _buffers[1];
                auto&[to, len] = buf->addr;
                std::memset(&to, 0, sizeof(to));
                len = 0;
                auto& sendbuf = buf->data;
                if( !(sendbuf.iov_base = std::malloc(MIN_BUFSIZE)) )
                    throw std::runtime_error("Unable to allocate space in send buffer.");
                sendbuf.iov_len = MIN_BUFSIZE;
                char *data = reinterpret_cast<char*>(sendbuf.iov_base);
                setp(data, data+MIN_BUFSIZE);
            }
        }
        sockbuf::sockbuf(): 
            Base(),
            _which{std::ios_base::in | std::ios_base::out},
            _buffers{}, _socket{0},
            _errno{0}, _connected{false}
        { _init_buf_ptrs(); }

        sockbuf::sockbuf(native_handle_type sockfd, bool connected, std::ios_base::openmode which):
            Base(), _which{which}, _buffers{},
            _socket{sockfd}, _errno{0}, _connected{connected}
        { _init_buf_ptrs(); }

        sockbuf::sockbuf(int domain, int type, int protocol, std::ios_base::openmode which):
            Base(), _which{which}, _buffers{},
            _socket{0}, _errno{0}, _connected{false}
        {
            if((_socket = socket(domain, type, protocol)) < 0) 
                throw std::runtime_error("Can't open socket.");
            _init_buf_ptrs();
        }
        sockbuf::buffer_type sockbuf::connectto(const struct sockaddr *addr, socklen_t addrlen){
            auto& addr_ = std::get<struct sockaddr_storage>(_buffers.back()->addr);
            if(addr_.ss_family != AF_UNSPEC){
                auto& piov = _buffers.back()->data;
                piov.iov_len = pptr()-pbase();
                _buffers.push_back(std::make_shared<socket_message>());
                auto& sendbuf = _buffers.back()->data;
                if( !(sendbuf.iov_base = std::malloc(MIN_BUFSIZE)) )
                    throw std::runtime_error("Unable to allocate space in send buffer.");
                sendbuf.iov_len = MIN_BUFSIZE;
                char *data = reinterpret_cast<char*>(sendbuf.iov_base);
                setp(data, data+MIN_BUFSIZE);
            }
            auto&[address, len] = _buffers.back()->addr;
            len = addrlen;
            std::memcpy(&address, addr, addrlen);
            while(!_connected && connect(_socket, addr, addrlen)){
                _errno = errno;
                switch(_errno){
                    case EINTR: continue;
                    case EISCONN:
                        _connected = true;
                    case EAGAIN:
                    case EALREADY:
                    case EINPROGRESS:
                        return _buffers.back();
                    default: 
                        return buffer_type();
                }
            }
            return _buffers.back();
        }
        sockbuf::pos_type sockbuf::seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which){
            pos_type pos = 0;
            switch(dir){
                case std::ios_base::beg:
                    return seekpos(pos+off, which);
                case std::ios_base::cur:
                case std::ios_base::end:
                    if(which & std::ios_base::in){
                        switch(dir) {
                            case std::ios_base::cur:
                                pos = gptr()-eback();
                                break;
                            case std::ios_base::end:
                                pos = egptr()-eback();
                                break;
                            default:
                                return Base::seekoff(off, dir, which);
                        }
                    } else if(which & std::ios_base::out)
                        pos = pptr()-pbase();
                    return seekpos(pos+off, which);
                default:
                    return Base::seekoff(off,dir,which);
            }
        }
        sockbuf::Base::pos_type sockbuf::seekpos(Base::pos_type pos, std::ios_base::openmode which){
            if(pos < 0)
                return Base::seekpos(pos, which);
            if(which & std::ios_base::out){
                if(pos > pptr()-pbase())
                    return Base::seekpos(pos, which);
                setp(pbase(), epptr());
                pbump(pos);
            } else if(which & std::ios_base::in){
                if(pos > egptr()-eback())
                    return Base::seekpos(pos, which);
                setg(eback(), eback()+pos, egptr());
            } else return Base::seekpos(pos, which);
            return pos;
        }
        void sockbuf::_resizewbuf(const buffer_type& buf, void *base, std::size_t buflen){
            auto& sendbuf = buf->data;
            if(base != nullptr){
                std::memmove(sendbuf.iov_base, base, buflen);
                if(buf==_buffers.back()){
                    setp(pbase(), epptr());
                    pbump(buflen);
                }
            }
            std::size_t size=MIN_BUFSIZE*(sendbuf.iov_len/MIN_BUFSIZE+1);
            std::streamsize off = pptr()-pbase();
            if( !(sendbuf.iov_base=std::realloc(sendbuf.iov_base, size)) )
                throw std::runtime_error("Unable to allocate space for send buffer.");
            if(buf == _buffers.back()){
                char *data = reinterpret_cast<char*>(sendbuf.iov_base);
                setp(data, data+size);
                pbump(off);
                sendbuf.iov_len = size;
            }
        }
        int sockbuf::_send(const buffer_type& buf){
            auto& header = buf->header;
            auto&[address, addrlen] = buf->addr;
            if(!_connected && address.ss_family == AF_UNSPEC){
                _resizewbuf(buf);
                return 0;
            } else if(_connected ||
                /* This next line is so that we can take advantage of *
                 * the connected() socket fast paths in the kernel ev-*
                 * en when using disconnected transports like UDP and *
                 * SOCK_DGRAM Unix domain sockets.                    */
                buf == _buffers.back()
            ){
                header.msg_name = nullptr;
                header.msg_namelen = 0;
            } else {
                header.msg_name = &address;
                header.msg_namelen = addrlen;
            }
            auto& sendbuf = buf->data;
            void *data = sendbuf.iov_base, *base = nullptr;
            std::size_t iov_len = sendbuf.iov_len;
            std::size_t buflen = (buf==_buffers.back()) ? pptr()-pbase() : iov_len;
            if(buflen){
                header.msg_iov = &sendbuf;
                header.msg_iovlen = 1;
            } else {
                header.msg_iov = nullptr;
                header.msg_iovlen = 0;
            }
            auto& cbuf = buf->ancillary;
            if(!cbuf.empty()){
                header.msg_control = cbuf.data();
                header.msg_controllen = cbuf.size();
            } else {
                header.msg_control = nullptr;
                header.msg_controllen = 0;
            }
            if(!buflen && cbuf.empty())
                return 0;
            sendbuf.iov_len = std::min(buflen, MIN_BUFSIZE);
            while(auto len = sendmsg(_socket, &header, MSG_DONTWAIT | MSG_NOSIGNAL)){
                if(len < 0){
                    _errno = errno;
                    switch(_errno){
                        case EISCONN:
                            _connected = true;
                            header.msg_name = nullptr;
                            header.msg_namelen = 0;
                        case EINTR: continue;
                        default:
                            base = sendbuf.iov_base;
                            sendbuf.iov_base = data;
                            sendbuf.iov_len = (buf==_buffers.back()) ? iov_len : buflen;
                            _resizewbuf(buf, base, buflen);
                            switch(_errno){
                                /* case EAGAIN: */
                                case EWOULDBLOCK: return 0;
                                default: return -1;
                            }
                    }
                }
                if(header.msg_control != nullptr){
                    header.msg_control = nullptr;
                    header.msg_controllen = 0;
                    cbuf.clear();
                    cbuf.shrink_to_fit();
                }
                if(!(buflen-=len))
                    break;
                sendbuf.iov_base = reinterpret_cast<char*>(sendbuf.iov_base)+len;
                sendbuf.iov_len = std::min(buflen, MIN_BUFSIZE);
            }
            base = sendbuf.iov_base;
            sendbuf.iov_base = data;
            sendbuf.iov_len = (buf==_buffers.back()) ? iov_len : buflen;
            _resizewbuf(buf, base, buflen);
            return 0;
        }
        int sockbuf::sync() {
            auto it = ++_buffers.begin();
            while(it < _buffers.end()){
                auto& buf = *it;
            SEND:
                while(_send(buf)){
                    auto&[address, addrlen] = buf->addr;
                    switch(_errno){
                        case ENOTCONN:
                            if(address.ss_family == AF_UNSPEC)
                                return 0;
                            while(connect(_socket, reinterpret_cast<const struct sockaddr*>(&address), addrlen)){
                                _errno = errno;
                                switch(_errno){
                                    case EINTR: continue;
                                    case EISCONN:
                                        _connected = true;
                                        goto SEND;
                                    case EALREADY:
                                    case EAGAIN:
                                    case EINPROGRESS:
                                        return 0;
                                    default:
                                        return -1;
                                }
                            }
                            continue;
                        default:
                            return -1;
                    }
                }
                if(buf->data.iov_len)
                    return 0;
                if(++it != _buffers.end()){
                    std::free(buf->data.iov_base);
                    it = _buffers.erase(--it);
                }
            }
            return 0;
        }
        void sockbuf::_memmoverbuf(){
            auto len = egptr()-gptr();
            std::memmove(eback(), gptr(), len);
            setg(eback(), eback(), eback()+len);
        }
        int sockbuf::_recv(){
            _memmoverbuf();
            auto& buf = _buffers.front();
            auto& header = buf->header;
            auto&[address, addrlen] = buf->addr;
            header.msg_name = &address;
            header.msg_namelen = addrlen;
            auto& recvbuf = buf->data;
            void *data = recvbuf.iov_base;
            std::size_t iov_len = recvbuf.iov_len;
            std::size_t buflen = iov_len-(egptr()-eback());
            if(buflen > 0){
                header.msg_iov = &recvbuf;
                header.msg_iovlen = 1;
            } else {
                header.msg_iov = nullptr;
                header.msg_iovlen = 0;
            }
            auto& cbuf = buf->ancillary;
            if(!cbuf.empty()){
                header.msg_control = cbuf.data();
                header.msg_controllen = cbuf.size();
            } else {
                header.msg_control = nullptr;
                header.msg_controllen = 0;
            }
            if(!buflen && cbuf.empty())
                return 0;
            recvbuf.iov_base = egptr();
            recvbuf.iov_len = buflen;
            while(auto len = recvmsg(_socket, &header, MSG_DONTWAIT)){
                if(len < 0){
                    _errno = errno;
                    switch(_errno){
                        case EINTR: continue;
                        default:
                            recvbuf.iov_base = data;
                            recvbuf.iov_len = iov_len;
                            switch(_errno){
                                /* case EAGAIN: */
                                case EWOULDBLOCK: return 0;
                                default: return -1;
                            }
                    }
                }
                setg(eback(), gptr(), egptr()+len);
                recvbuf.iov_base = data;
                recvbuf.iov_len = iov_len;
                return 0;
            }
            recvbuf.iov_base = data;
            recvbuf.iov_len = iov_len;
            if(header.msg_control != nullptr)
                return 0;
            return -1;
        }
        std::streamsize sockbuf::showmanyc() {
            if(!(egptr()-gptr()) && _recv())
                return -1;
            return egptr()-gptr();
        }
        std::streamsize sockbuf::xsputn(const char_type *s, std::streamsize count){
            std::streamsize len=0, n=0;
            while((s+=n) && (len+=n) < count){
                while( !(n=std::min(epptr()-pptr(), count-len)) )
                    if(sync())
                        return len;
                std::memcpy(pptr(), s, n);
                pbump(n);
            }
            return len;
        }
        sockbuf::int_type sockbuf::overflow(sockbuf::int_type ch){
            if(pbase() == nullptr || sync())
                return traits_type::eof();
            if(!traits_type::eq_int_type(ch, traits_type::eof()))
                return sputc(ch);
            return ch;
        }
        sockbuf::int_type sockbuf::underflow() {
            if(eback() == nullptr || _recv())
                return traits_type::eof();
            if(egptr()-eback())
                return traits_type::to_int_type(*gptr());
            if(_poll(_socket, POLLIN))
                return traits_type::eof();
            return underflow();
        }
        std::streamsize sockbuf::xsgetn(char_type *s, std::streamsize count){
            std::streamsize len=0, n=0;
            while((s+=n) && (len+=n) < count){
                while( !(n = std::min(egptr()-gptr(), count-len)) ){
                    if(_recv() ||
                        (!(egptr()-eback()) &&
                            _poll(_socket,POLLIN)))
                        return len;
                }
                std::memcpy(s, gptr(), n);
                gbump(n);
            }
            return len;
        }
        sockbuf::~sockbuf(){
            for(auto& buf: _buffers)
                std::free(buf->data.iov_base);
            if(_socket > 2) close(_socket);
        }
    }
}
