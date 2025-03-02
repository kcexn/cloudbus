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
            struct pollfd fds[1] = {};
            fds[0] = {
                socket,
                events
            };
            if(poll(fds, 1, -1) < 0){
                switch(errno){
                    case EINTR:
                        return _poll(socket, events);
                    default:
                        return -1;
                }
            } else if(fds[0].revents & (POLLHUP | POLLERR))
                return -1;
            return 0;
        }
        void sockbuf::_init_buf_ptrs(){
            if(_which & std::ios_base::in){
                std::get<socklen_t>(_addresses[0]) = sizeof(struct sockaddr_storage);
                auto& buf = _buffers[0];
                buf.resize(MIN_BUFSIZE);
                Base::setg(buf.data(), buf.data(), buf.data());
            }
            if(_which & std::ios_base::out){
                auto& buf = _buffers[1];
                buf.resize(MIN_BUFSIZE);
                Base::setp(buf.data(), buf.data() + buf.size());
            }
        }
        
        int sockbuf::_send(char_type *buf, size_type size){
            struct msghdr msgh = {};
            auto&[addr, addrlen] = _addresses[1];
            if(!_connected && addr.ss_family != AF_UNSPEC){
                msgh.msg_name = &addr;
                msgh.msg_namelen = addrlen;
            }
            struct iovec iov = {buf, std::min(MIN_BUFSIZE, size)};
            if(size){
                msgh.msg_iov = &iov;
                msgh.msg_iovlen = 1;
            }
            auto& cbuf = _cbufs[1];
            if(!cbuf.empty()){
                msgh.msg_control = cbuf.data();
                msgh.msg_controllen = cbuf.size();
            }
            if(size == 0 && cbuf.empty()) return 0;
            while(auto len = sendmsg(_socket, &msgh, MSG_DONTWAIT | MSG_NOSIGNAL)){
                if(len < 0){
                    _errno = errno;
                    switch(_errno){
                        case EISCONN:
                            _connected = true;
                            msgh.msg_name = nullptr;
                            msgh.msg_namelen = 0;
                        case EINTR: continue;
                        case EWOULDBLOCK:
                            std::memmove(Base::pbase(), buf, size);
                            Base::setp(Base::pbase(), Base::epptr());
                            Base::pbump(size);
                            return 0;
                        default:
                            std::memmove(Base::pbase(), buf, size);
                            Base::setp(Base::pbase(), Base::epptr());
                            Base::pbump(size);
                            return -1;
                    }
                }
                if(static_cast<size_type>(len) == size){
                    Base::setp(Base::pbase(), Base::epptr());
                    return 0;
                }                
                if(msgh.msg_control != nullptr){
                    msgh.msg_control = nullptr;
                    msgh.msg_controllen = 0;
                }
                buf += len;
                size -= len;
                iov.iov_base = buf;
                iov.iov_len = std::min(MIN_BUFSIZE, size);
            }
            std::memmove(Base::pbase(), buf, size);
            Base::setp(Base::pbase(), Base::epptr());
            Base::pbump(size);
            return 0;
        }
        int sockbuf::_recv(){
            auto& buf = _buffers[0];
            struct iovec iov = {
                Base::egptr(),
                buf.size()-(Base::egptr()-Base::eback())
            };
            auto&[addr, addrlen] = _addresses[0];
            struct msghdr msgh = {
                &addr, addrlen,
                &iov, 1,
                nullptr, 0
            };
            auto& cbuf = _cbufs[0];
            if(!cbuf.empty()){
                msgh.msg_control = cbuf.data();
                msgh.msg_controllen = cbuf.size();
            }
            while(auto len = recvmsg(_socket, &msgh, MSG_DONTWAIT)){
                if(len < 0){
                    _errno = errno;
                    switch(_errno){
                        case EINTR: continue;
                        case EWOULDBLOCK: return 0;
                        default: return -1;
                    }
                }
                Base::setg(Base::eback(), Base::gptr(), Base::egptr()+len);
                return 0;
            }
            return -1;
        }
        void sockbuf::_memmoverbuf(){
            auto len = Base::egptr()-Base::gptr();
            std::memmove(Base::eback(), Base::gptr(), len);
            Base::setg(Base::eback(), Base::eback(), Base::eback()+len);
        }
        void sockbuf::_resizewbuf(){
            auto& buf = _buffers[1];
            auto pos = Base::pptr()-Base::pbase();
            auto n = pos/MIN_BUFSIZE + 1;
            buf.resize(n*MIN_BUFSIZE);
            buf.shrink_to_fit();
            Base::setp(buf.data(), buf.data()+buf.size());
            Base::pbump(pos);
        }
        sockbuf::Base::pos_type sockbuf::seekoff(Base::off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which){
            Base::pos_type pos = 0;
            switch(dir){
                case std::ios_base::beg:
                    if(off < 0) 
                        return Base::seekoff(off, dir, which);
                    pos += off;
                    return seekpos(pos, which);
                case std::ios_base::cur:
                    if(which & std::ios_base::out){
                        if(off > 0 || (off < 0 && Base::pptr()-Base::pbase() < -off))
                            return Base::seekoff(off, dir, which);
                        pos = Base::pptr()-Base::pbase()+off;
                    } else if(which & std::ios_base::in){
                        if(off > 0 && Base::egptr() - Base::gptr() < off)
                            return Base::seekoff(off, dir, which);
                        else if(off < 0 && Base::gptr() - Base::eback() < -off)
                            return Base::seekoff(off, dir, which);
                        pos = Base::gptr()-Base::eback()+off;
                    }
                    return seekpos(pos, which);               
                case std::ios_base::end:
                    if(off > 0)
                        return Base::seekoff(off, dir, which);
                    if(which & std::ios_base::out)
                        pos = Base::egptr()-Base::eback()+off;
                    else if(which & std::ios_base::in)
                        pos = Base::epptr()-Base::pbase()+off;
                    return seekpos(pos, which);
                default:
                    return Base::seekoff(off,dir,which);
            }
        }
        sockbuf::Base::pos_type sockbuf::seekpos(Base::pos_type pos, std::ios_base::openmode which){
            if(which & std::ios_base::out){
                if(Base::epptr()-Base::pbase() < pos)
                    return Base::seekpos(pos, which);
                Base::setp(Base::pbase(), Base::epptr());
                Base::pbump(pos);
                return pos;
            } else if(which & std::ios_base::in){
                if(Base::egptr()-Base::eback() < pos)
                    return Base::seekpos(pos, which);
                Base::setg(Base::eback(), Base::eback()+pos, Base::egptr());
                return pos;
            } else return Base::seekpos(pos, which);
        }
        int sockbuf::sync() {
            if(_which & std::ios_base::out){
                auto size = Base::pptr()-Base::pbase();
                if(_send(Base::pbase(), size))
                    return -1;
                _resizewbuf();
            }
            return 0;
        }
        std::streamsize sockbuf::showmanyc() {
            if(Base::gptr() != Base::eback())
                _memmoverbuf();
            if(_recv() && Base::egptr()==Base::gptr())
                return -1;
            return Base::egptr()-Base::gptr();
        }
        std::streamsize sockbuf::xsputn(const char *s, std::streamsize count){
            if(count == 0) return count;
            std::streamsize remainder = Base::epptr()-Base::pptr();
            auto len = std::min(remainder, count);
            std::memcpy(Base::pptr(), s, len);
            Base::pbump(len);
            if(len == count) 
                return len;
            if(Base::epptr()==Base::pptr()){
                auto *ch = s+len;
                if(traits_type::eq_int_type(overflow(*ch), traits_type::eof()))
                    return len;
                return len + xsputn(++ch, count-(len+1));
            }
            return len;
        }
        sockbuf::int_type sockbuf::overflow(sockbuf::int_type ch){
            if(Base::pbase() == nullptr)
                return traits_type::eof();
            if(sync()) {
                auto&[addr, addrlen] = _addresses[1];
                switch(_errno){
                    case ENOTCONN:
                        if(addr.ss_family == AF_UNSPEC) return traits_type::eof();
                        if(connectto(reinterpret_cast<const struct sockaddr*>(&addr), addrlen)){
                            switch(_errno) {
                                case EALREADY:
                                case EAGAIN:
                                case EINPROGRESS:
                                    _resizewbuf();
                                    if(!traits_type::eq_int_type(ch, traits_type::eof())) 
                                        return Base::sputc(ch);
                                    return ch;
                                case EISCONN:
                                    return overflow(ch);
                                default:
                                    return traits_type::eof();
                            }
                        } else return overflow(ch);
                    default:
                        return traits_type::eof();
                }
            }
            if(Base::pptr()==Base::epptr()){
                if(_poll(_socket, POLLOUT)) 
                    return traits_type::eof();
                else return overflow(ch);
            }
            if(!traits_type::eq_int_type(ch, traits_type::eof()))
                return Base::sputc(ch);
            return ch;
        }
        sockbuf::int_type sockbuf::underflow() {
            if(Base::eback() == nullptr) 
                return traits_type::eof();
            if(Base::gptr() != Base::eback())
                _memmoverbuf();
            if(_recv())
                return traits_type::eof();
            if(Base::gptr()==Base::egptr()){
                if(_poll(_socket, POLLIN)) 
                    return traits_type::eof();
                return underflow();
            }
            return traits_type::to_int_type(*Base::gptr());
        }

        sockbuf::sockbuf(): 
            Base(),
            _which{std::ios_base::in | std::ios_base::out},
            _buffers{}, _cbufs{},
            _addresses{}, _socket{}
        { _init_buf_ptrs(); }
        sockbuf::sockbuf(sockbuf&& other):
            Base(std::move(other)),
            _which{std::move(other._which)},
            _buffers{std::move(other._buffers)},
            _cbufs{std::move(other._cbufs)},
            _addresses{std::move(other._addresses)},
            _socket{std::move(other._socket)}
        { other._socket = 0; }

        sockbuf& sockbuf::operator=(sockbuf&& other){
            _which = std::move(other._which);
            _buffers = std::move(other._buffers);
            _cbufs = std::move(other._cbufs);
            _addresses = std::move(other._addresses);
            _socket = std::move(other._socket);
            auto& wbuf = _buffers[1];
            setp(wbuf.data(), wbuf.data()+wbuf.size());
            pbump(other.pptr() - other.pbase());
            auto& rbuf = _buffers[0];
            auto goff = other.gptr() - other.eback();
            auto egoff = other.egptr() - other.eback();
            setg(rbuf.data(), rbuf.data() + goff, rbuf.data() + egoff);
            other.setp(nullptr, nullptr);
            other.setg(nullptr, nullptr, nullptr);
            other._socket = 0;
            return *this;
        }

        sockbuf::sockbuf(native_handle_type sockfd, std::ios_base::openmode which):
            Base(), _which{which},
            _buffers{}, _cbufs{},
            _addresses{}, _socket{sockfd}
        { _init_buf_ptrs(); }
        sockbuf::sockbuf(int domain, int type, int protocol, std::ios_base::openmode which):
            Base(), _which{which},
            _buffers{}, _cbufs{},
            _addresses{}, _socket{}
        {
            if((_socket = socket(domain, type, protocol)) < 0) 
                throw std::runtime_error("Can't open socket.");
            _init_buf_ptrs();
        }
        int sockbuf::connectto(const struct sockaddr* addr, socklen_t addrlen){
            int ret = 0;
            if(connect(_socket, addr, addrlen)){
                _errno = errno;
                switch(_errno){
                    case EINTR:
                        return connectto(addr, addrlen);
                    default:
                        ret = -1;
                        break;
                }
            }
            auto&[dst, dstlen] = _addresses[1];
            dstlen = addrlen;
            std::memcpy(&dst, addr, dstlen);
            return ret;
        }
        sockbuf::~sockbuf(){
            if(_socket > 2) close(_socket);
        }
    }
}
