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
            for(std::size_t i=0; i < 2; ++i)
                _buffers.push_back(std::make_shared<socket_message>());
            if(_which & std::ios_base::in){
                auto& buf = _buffers[0];
                auto&[from, len] = buf->addr;
                len = sizeof(from);
                std::memset(&from, 0, sizeof(from));
                auto&[iov, recvbuf] = buf->data;
                recvbuf.resize(MIN_BUFSIZE);
                setg(recvbuf.data(), recvbuf.data(), recvbuf.data());
                iov.iov_base = egptr();
                iov.iov_len = recvbuf.size();
            }
            if(_which & std::ios_base::out){
                auto& buf = _buffers[1];
                auto&[to, len] = buf->addr;
                std::memset(&to, 0, sizeof(to));
                len = 0;
                auto&[iov, sendbuf] = buf->data;
                sendbuf.resize(MIN_BUFSIZE);
                setp(sendbuf.data(), sendbuf.data()+sendbuf.size());
                iov.iov_base = pbase();
                iov.iov_len = 0;
            }
        }      
        sockbuf::sockbuf(): 
            Base(),
            _which{std::ios_base::in | std::ios_base::out},
            _buffers{}, _socket{0},
            _errno{0}, _connected{false}
        { _init_buf_ptrs(); }

        sockbuf::sockbuf(native_handle_type sockfd, std::ios_base::openmode which):
            Base(), _which{which}, _buffers{},
            _socket{sockfd}, _errno{0}, _connected{false}
        { _init_buf_ptrs(); }

        sockbuf::sockbuf(int domain, int type, int protocol, std::ios_base::openmode which):
            Base(), _which{which}, _buffers{},
            _socket{0}, _errno{0}, _connected{false}
        {
            if((_socket = socket(domain, type, protocol)) < 0) 
                throw std::runtime_error("Can't open socket.");
            _init_buf_ptrs();
        }
        sockbuf::sockbuf(const sockbuf& other):
            Base(other), _which{other._which}, 
            _buffers{other._buffers}, _socket{other._socket}, 
            _errno{other._errno}, _connected{other._connected}
        {}

        sockbuf& sockbuf::operator=(const sockbuf& other){
            _which = other._which;
            _buffers = other._buffers;
            _socket = other._socket;
            _errno = other._errno;
            _connected = other._connected;
            Base::operator=(other);
            return *this;
        }
        void sockbuf::swap(sockbuf& other){
            auto buf = _buffers;
            auto which = _which;
            auto sock = _socket;
            auto tmperr = _errno;
            auto conn = _connected;
            _buffers = other._buffers;
            _which = other._which;
            _socket = other._socket;
            _errno = other._errno;
            _connected = other._connected;
            other._buffers = buf;
            other._which = which;
            other._socket = sock;
            other._errno = tmperr;
            other._connected = conn;
            return Base::swap(other);
        }
        sockbuf::buffer_type sockbuf::connectto(const struct sockaddr* addr, socklen_t addrlen){
            auto& addr_ = std::get<struct sockaddr_storage>(_buffers.back()->addr);
            if(addr_.ss_family != AF_UNSPEC){
                auto& piov = std::get<struct iovec>(_buffers.back()->data);
                piov.iov_base = pbase();
                piov.iov_len = pptr()-pbase();
                _buffers.push_back(std::make_shared<socket_message>());
                auto&[iov, sendbuf] = _buffers.back()->data;
                sendbuf.resize(MIN_BUFSIZE);
                setp(sendbuf.data(), sendbuf.data()+sendbuf.size());
                iov.iov_base = pbase();
                iov.iov_len = 0;
            }
            auto&[address, len] = _buffers.back()->addr;
            len = addrlen;
            std::memcpy(&address, addr, addrlen);
            while(!_connected && connect(_socket, addr, addrlen)){
                _errno = errno;
                switch(_errno){
                    case EINTR: continue;
                    default: return buffer_type();
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
        void sockbuf::_resizewbuf(const buffer_type& buf){
            auto&[iov, sendbuf] = buf->data;
            auto n = iov.iov_len/MIN_BUFSIZE + 1;
            std::memmove(sendbuf.data(), iov.iov_base, iov.iov_len);
            sendbuf.resize(n*MIN_BUFSIZE);
            sendbuf.shrink_to_fit();
            iov.iov_base = sendbuf.data();
            if(buf == _buffers.back()){
                setp(sendbuf.data(), sendbuf.data()+sendbuf.size());
                pbump(iov.iov_len);
            }
        }
        int sockbuf::_send(const buffer_type& buf){
            auto& header = buf->header;
            auto&[address, addrlen] = buf->addr;
            if(!_connected && address.ss_family != AF_UNSPEC){
                header.msg_name = &address;
                header.msg_namelen = addrlen;
            } else {
                header.msg_name = nullptr;
                header.msg_namelen = 0;
            }
            auto&[iov, sendbuf] = buf->data;
            auto size = iov.iov_len;
            iov.iov_len = std::min(size, MIN_BUFSIZE);
            if(iov.iov_len){
                header.msg_iov = &iov;
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
            if(iov.iov_len == 0 && cbuf.empty())
                return 0;
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
                            iov.iov_len = size;
                            switch(_errno){
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
                if(!(size -= len))
                    break;
                iov.iov_base = reinterpret_cast<char *>(iov.iov_base) + len;
                iov.iov_len = std::min(size, MIN_BUFSIZE);
            }
            iov.iov_len = size;
            return 0;
        }
        int sockbuf::sync() {
            auto it = _buffers.begin() + 1;
            while(it < _buffers.end()){
                auto& buf = *it;
                if(buf == _buffers.back()){
                    auto& iov = std::get<struct iovec>(buf->data);
                    iov.iov_base = pbase();
                    iov.iov_len = pptr()-pbase();
                }
                while(_send(buf)){
                    auto&[address, addrlen] = buf->addr;
                    switch(_errno){
                        case ENOTCONN:
                            if(address.ss_family != AF_UNSPEC && connect(_socket, reinterpret_cast<const struct sockaddr*>(&address), addrlen)){
                                _errno = errno;
                                switch(_errno){
                                    case EALREADY:
                                    case EAGAIN:
                                    case EINPROGRESS:
                                        _resizewbuf(buf);
                                        return 0;
                                    case EISCONN: continue;
                                    default:
                                        _resizewbuf(buf);
                                        return -1;
                                }
                            } else continue;
                        default:
                            _resizewbuf(buf);
                            return -1;
                    }
                }
                if(it != _buffers.end()-1)
                    it = _buffers.erase(it);
                else ++it;
            }
            _resizewbuf(_buffers.back());
            return 0;
        }
        void sockbuf::_memmoverbuf(){
            auto&[iov, recvbuf] = _buffers[0]->data;
            auto len = egptr()-gptr();
            std::memmove(eback(), gptr(), len);
            setg(eback(), eback(), eback()+len);
            iov.iov_base = egptr();
            iov.iov_len = recvbuf.size() - len;
        }
        int sockbuf::_recv(){
            _memmoverbuf();
            auto& buf = _buffers[0];
            auto& header = buf->header;
            auto&[address, addrlen] = buf->addr;
            header.msg_name = &address;
            header.msg_namelen = addrlen;
            auto&[iov, recvbuf] = buf->data;
            if(iov.iov_len > 0){
                header.msg_iov = &iov;
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
            if(iov.iov_len == 0 && cbuf.empty())
                return 0;
            while(auto len = recvmsg(_socket, &header, MSG_DONTWAIT)){
                if(len < 0){
                    _errno = errno;
                    switch(_errno){
                        case EINTR: continue;
                        case EWOULDBLOCK: return 0;
                        default: return -1;
                    }
                }
                iov.iov_base = reinterpret_cast<char*>(iov.iov_base) + len;
                iov.iov_len -= len;
                setg(eback(), gptr(), egptr()+len);
                return 0;
            }
            if(header.msg_control != nullptr)
                return 0;
            return -1;
        }        
        std::streamsize sockbuf::showmanyc() {
            if(auto len = egptr()-gptr(); !len && _recv())
                return -1;
            return egptr()-gptr();
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
            if(gptr()==egptr()){
                if(_poll(_socket, POLLIN))
                    return traits_type::eof();
                else return underflow();
            }
            return traits_type::to_int_type(*Base::gptr());
        }
        sockbuf::~sockbuf(){
            if(_socket > 2) close(_socket);
        }
    }
}
