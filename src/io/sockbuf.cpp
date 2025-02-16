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
        using native_handle_type = sockbuf::native_handle_type;
        using sockaddr_t = struct sockaddr;
        using sockaddr_storage = struct sockaddr_storage;   
        
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
        
        static optval socket_name(native_handle_type sockfd, optval& val){
            void *opts[2] = {};
            int status = 0;
            optval status_(sizeof(int));
            
            std::memcpy(opts, val.data(), val.size());
            status = getsockname(sockfd, reinterpret_cast<struct sockaddr*>(opts[0]), reinterpret_cast<socklen_t*>(opts[1]));
            std::memcpy(status_.data(), &status, sizeof(int));
            return status_;
        }
        
        static optval socket_accept(native_handle_type sockfd, optval& val){
            void *opts[2] = {};
            native_handle_type fd = 0;
            optval fd_(sizeof(native_handle_type));
            
            if(val.size() == 2*sizeof(void*)) std::memcpy(opts, val.data(), val.size());
            fd = accept(sockfd, reinterpret_cast<struct sockaddr*>(opts[0]), reinterpret_cast<socklen_t*>(opts[1]));
            std::memcpy(fd_.data(), &fd, sizeof(native_handle_type));
            return fd_;
        }
            
        static void socket_bind(native_handle_type socket, optval& val){
            sockaddr_t *addr = nullptr;
            std::memcpy(&addr, val.data(), val.size());
            socklen_t size = 0;
            switch(addr->sa_family){
                case AF_UNIX:
                    size = sizeof(struct sockaddr_un);
                    break;
                default:    
                    throw std::runtime_error("Unknown socket domain.");
            }
            if(bind(socket, addr, size))
                throw std::runtime_error("unable to bind the socket");
        }
        
        static void socket_listen(native_handle_type socket, optval& val){
            int *backlog = reinterpret_cast<int*>(val.data());
            if(listen(socket, *backlog)) throw std::runtime_error("Unable to listen on socket.");
        }
        
        static std::size_t getbuflen(std::vector<std::vector<char> >& buffers, const char* base){
            auto it = std::find_if(buffers.cbegin(), buffers.cend(), [&](const auto& buf){ return buf.data() == base; });
            if(it == buffers.cend()) return SIZE_MAX;
            return it->size();
        }
        
        void sockbuf::_init_buf_ptrs(){
            if(_which & std::ios_base::in){
                _buffers.emplace_back();
                auto& buf = _buffers.back();
                buf.resize(BUFSIZE);
                Base::setg(buf.data(), buf.data(), buf.data()); 
            }
            if(_which & std::ios_base::out){
                _buffers.emplace_back();
                auto& buf = _buffers.back();
                buf.resize(BUFSIZE);
                Base::setp(buf.data(), buf.data() + buf.size()); 
            }
        }
        
        int sockbuf::_send(char_type *buf, size_type size){
            iovec& iov = _iov[1];
            struct msghdr *msgptr = &_msghdrs[1];
            auto& address = std::get<sockaddr_storage>(_addresses[1]);
            if(!_connected && address.ss_family != AF_UNSPEC){
                auto& addrlen = std::get<socklen_t>(_addresses[1]);
                msgptr->msg_name = &address;
                msgptr->msg_namelen = addrlen;
            } else {
                msgptr->msg_name = nullptr;
                msgptr->msg_namelen = 0;
            }
            if(size > 0){
                iov.iov_base = buf;
                iov.iov_len = size;
                msgptr->msg_iov = &iov;
                msgptr->msg_iovlen = 1;
            } else {
                iov.iov_base = nullptr;
                iov.iov_len = 0;
                msgptr->msg_iov = nullptr;
                msgptr->msg_iovlen = 0;
            }
            
            std::streamsize len = sendmsg(_socket, msgptr, MSG_DONTWAIT | MSG_NOSIGNAL);
            while(len >= 0){
                if(msgptr->msg_control != nullptr){
                    msgptr->msg_control = nullptr;
                    msgptr->msg_controllen = 0;
                }
                if(static_cast<std::size_t>(len) == size) {
                    Base::setp(Base::pbase(), Base::epptr());
                    return 0;
                }
                buf += len;
                size -= len;
                iov.iov_base = buf;
                iov.iov_len = size;
                len = sendmsg(_socket, msgptr, MSG_DONTWAIT | MSG_NOSIGNAL);
            }
            if(len < 0){
                _errno = errno;
                switch(_errno){
                    case EISCONN:
                        _connected = true;
                    case EINTR:
                        return _send(buf, size);
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
            Base::setp(Base::pbase(), Base::epptr());
            return 0;
        }

        int sockbuf::_recv(){
            iovec& iov = _iov[0];
            struct msghdr *msgptr = &_msghdrs[0];
            size_type buflen = getbuflen(_buffers, Base::eback());
            if(buflen == SIZE_MAX) return -1;
            iov.iov_base = Base::egptr();
            iov.iov_len = Base::eback() + buflen - Base::egptr();
            
            msgptr->msg_name = &(std::get<sockaddr_storage>(_addresses[0]));
            msgptr->msg_namelen = sizeof(sockaddr_storage);
            msgptr->msg_iov = &iov;
            msgptr->msg_iovlen = 1;
            if(_cbufs[0].size() > 0){
                msgptr->msg_control = _cbufs[0].data();
                msgptr->msg_controllen = _cbufs[0].size();
            } else {
                msgptr->msg_control = nullptr;
                msgptr->msg_controllen = 0;
            }
            std::streamsize len = recvmsg(_socket, msgptr, MSG_DONTWAIT);
            while(len < 0){
                _errno = errno;
                switch(_errno){
                    case EINTR:
                        len = recvmsg(_socket, msgptr, MSG_DONTWAIT);
                        break;
                    case EWOULDBLOCK:
                        return 0;
                    default:
                        return -1;
                }
            }
            if(len == 0) return -1;
            Base::setg(Base::eback(), Base::gptr(), Base::egptr()+len);
            return 0;
        }
        
        void sockbuf::_memmoverbuf(){
            auto ga = Base::egptr() - Base::gptr();
            auto oldarea = Base::gptr() - Base::eback();
            auto *nxtegptr = Base::eback();
            if(ga > 0) {
                if(ga < oldarea){
                    std::memcpy(Base::eback(), Base::gptr(), ga);
                } else {
                    std::memmove(Base::eback(), Base::gptr(), ga);
                }
                nxtegptr += ga;
            }
            Base::setg(Base::eback(), Base::eback(), nxtegptr);
        }
        
        void sockbuf::_resizewbuf(){
            auto it = std::find_if(_buffers.begin(), _buffers.end(), [&](auto& buf){ return buf.data() == Base::pbase(); });
            if(it == _buffers.end()) throw std::runtime_error("Write buffer could not be found.");
            std::size_t off = Base::pptr() - Base::pbase();
            if(off < BUFSIZE-1 && it->size() > BUFSIZE){
                it->resize(BUFSIZE);
                it->shrink_to_fit();
                Base::setp(it->data(), it->data() + it->size());
                Base::pbump(off);
            } else if(Base::pptr() == Base::epptr()) {
                it->resize(2*(it->size()));
                Base::setp(it->data(), it->data() + it->size());
                Base::pbump(off);
            }
        }

        sockbuf::Base::pos_type sockbuf::seekoff(Base::off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which){
            Base::pos_type pos = 0;
            switch(dir){
                case std::ios_base::beg:
                    if(off < 0) break;
                    pos += off;
                    return seekpos(pos, which);
                case std::ios_base::end:
                    if(off > 0) break;
                    if(which & std::ios_base::in){
                        pos = Base::egptr() - Base::eback() + off;
                    } else if (which & std::ios_base::out){
                        pos = Base::epptr() - Base::pbase() + off;
                    }
                    return seekpos(pos, which);
                case std::ios_base::cur:
                    if(which & std::ios_base::in){
                        if(Base::gptr() + off > Base::egptr()) break;
                        if(off < 0 && Base::gptr() - Base::eback() > -off) break;
                        pos = Base::gptr() - Base::eback() + off;
                    } else if (which & std::ios_base::out){
                        if(off > 0 || (off < 0 && Base::pptr() - Base::pbase() > -off)) break;
                        pos = Base::pptr() - Base::pbase() + off;
                    }
                    return seekpos(pos, which);
                default:
                    break;
            }
            return Base::seekoff(off, dir, which);
        }

        sockbuf::Base::pos_type sockbuf::seekpos(Base::pos_type pos, std::ios_base::openmode which){
            if(which & std::ios_base::in){
                if(Base::eback()+pos <= Base::egptr()){
                    Base::setg(Base::eback(), Base::eback()+pos, Base::egptr());
                    return pos;
                }
            } else if (which & std::ios_base::out){
                if(Base::pbase() + pos <= Base::epptr()){
                    Base::setp(Base::pbase(), Base::epptr());
                    Base::pbump(pos);
                    return pos;
                }
            }
            return Base::seekpos(pos, which);
        }

        int sockbuf::sync() {
            if(_which & std::ios_base::out){
                std::size_t size = Base::pptr()-Base::pbase();
                if(size > 0 || _cbufs[1].size() > 0)
                    if(_send(Base::pbase(), size)) return -1;
                _resizewbuf();
            } else if(_which & std::ios_base::in){
                if(Base::gptr() != Base::eback()) _memmoverbuf();
                if(_recv()) return -1;
            }
            return 0;
        }
        
        std::streamsize sockbuf::showmanyc() {
            auto which_ = _which;
            _which &= ~std::ios_base::out;
            if(sync()) {
                _which = which_;
                return -1;
            }
            _which = which_;
            return Base::egptr() - Base::gptr();
        }
        
        sockbuf::int_type sockbuf::overflow(sockbuf::int_type ch){
            if(Base::pbase() == nullptr) return traits_t::eof();
            if(sync()) {
                auto& addr = _addresses[1];
                auto *dst = &(std::get<sockaddr_storage>(addr));
                auto& len = std::get<socklen_t>(addr);
                socklen_t optsize = sizeof(int);
                switch(_errno){
                    case ENOTCONN:
                        if(dst->ss_family == AF_UNSPEC) return traits_t::eof();
                        if(connectto(reinterpret_cast<const struct sockaddr*>(dst), len)){
                            switch(_errno) {
                                case EALREADY:
                                case EAGAIN:
                                case EINPROGRESS:
                                    if(_poll(_socket, POLLOUT)) return traits_t::eof();
                                    else if(getsockopt(_socket, SOL_SOCKET, SO_ERROR, &_errno, &optsize)) return traits_t::eof();
                                    else if(_errno) return traits_t::eof();
                                case EISCONN:
                                    return overflow(ch);
                                default:
                                    return traits_t::eof();
                            }
                        } else return overflow(ch);
                    default:
                        return traits_t::eof();
                }
            }
            if(Base::pptr() == Base::epptr()){
                if(_poll(_socket, POLLOUT)) return traits_t::eof();
                else return overflow(ch);
            }
            if(!traits_t::eq_int_type(ch, traits_t::eof())) return Base::sputc(ch);
            else return ch;
        }
        
        sockbuf::int_type sockbuf::underflow() {
            if(Base::eback() == nullptr) return traits_t::eof();
            auto which_ = _which;
            _which &= ~std::ios_base::out;
            if(sync()) {
                _which = which_;
                return traits_t::eof();
            }
            _which = which_;
            if(Base::gptr() == Base::egptr()) {
                if(_poll(_socket, POLLIN)) return traits_t::eof();
                return underflow();
            }
            return traits_t::to_int_type(*Base::gptr());
        }

        sockbuf::sockbuf()
            : Base(),
                BUFSIZE{DEFAULT_BUFSIZE},
                _which{std::ios_base::in | std::ios_base::out},
                _buffers{},
                _msghdrs{},
                _addresses{},
                _socket{}
        {
            _init_buf_ptrs();
        }
        
        sockbuf::sockbuf(sockbuf&& other):
            Base(std::move(other)),
            BUFSIZE{std::move(other.BUFSIZE)},
            _which{std::move(other._which)},
            _buffers{std::move(other._buffers)},
            _cbufs{std::move(other._cbufs)},
            _msghdrs{std::move(other._msghdrs)},
            _addresses{std::move(other._addresses)},
            _socket{std::move(other._socket)}
        {
            other._socket = 0;
        }

        sockbuf& sockbuf::operator=(sockbuf&& other){
            BUFSIZE = std::move(other.BUFSIZE);
            _which = std::move(other._which);
            _buffers = std::move(other._buffers);
            _cbufs = std::move(other._cbufs);
            _msghdrs = std::move(other._msghdrs);
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
            Base(),
            BUFSIZE{DEFAULT_BUFSIZE},
            _which{which},
            _socket{sockfd}
        {
            _init_buf_ptrs();
        }
        
        sockbuf::sockbuf(int domain, int type, int protocol, std::initializer_list<sockopt> l, std::ios_base::openmode which):
            Base(),
            BUFSIZE{DEFAULT_BUFSIZE},
            _which{which}
        {
            if((_socket = socket(domain, type, protocol)) < 0) throw std::runtime_error("Can't open socket.");
            for(auto& opt : l){
                try{
                    setopt(opt);
                } catch (const std::runtime_error& e) {
                    close(_socket);
                    throw e;
                }
            }
            _init_buf_ptrs();
        }

        
        void sockbuf::setopt(sockopt opt){
            optname& name = std::get<0>(opt);
            optval& val = std::get<1>(opt);
            std::transform(name.begin(), name.end(), name.begin(), [](char c){ return std::toupper(c); });
            if(name == "BIND") socket_bind(_socket, val);
            if(name == "LISTEN") socket_listen(_socket, val);
        }
        
        optval sockbuf::getopt(sockopt opt){
            optname& name = std::get<optname>(opt);
            optval& val = std::get<optval>(opt);
            std::transform(name.begin(), name.end(), name.begin(), [](char c){ return std::toupper(c); });
            if(name == "ACCEPT") return socket_accept(_socket, val);
            if(name == "SOCKNAME") return socket_name(_socket, val);
            return {};
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
            _connected = true;
            auto& destination = _addresses[1];
            auto *d_addr = &(std::get<sockaddr_storage>(destination));
            auto& d_len = std::get<socklen_t>(destination);
            d_len = addrlen;
            std::memcpy(d_addr, addr, d_len);
            return ret;
        }

        sockbuf::~sockbuf(){
            if(_socket > 2) close(_socket);
        }
    }
}
