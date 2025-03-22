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
#include <stdexcept>
#include <cctype>
#include <cstring>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
namespace io{
    namespace buffers{
        static int _poll(int *pipe, short events){
            auto rfd = pipe[0];
            auto wfd = pipe[1];
            struct pollfd fds[1] = {};
            auto& pfd = fds[0];
            if(events & POLLIN) pfd.fd = rfd;
            if(events & POLLOUT) pfd.fd = wfd;
            pfd.events = events;
            if(poll(fds, 1, -1) > 0){
                auto& revents = pfd.revents;
                if(revents & (POLLHUP | POLLERR)) return -1;
            }
            return 0;
        }
        
        pipebuf::pipebuf(pipebuf&& other):
            Base(other),
            _which{std::move(other._which)},
            _read{std::move(other._read)},
            _write{std::move(other._write)},
            _pipe{std::move(other._pipe)},
            BUFSIZE{std::move(other.BUFSIZE)}
        {
            other._pipe = {};
        }
        
        pipebuf::pipebuf(std::ios_base::openmode which):
            Base(),
            _which{which},
            BUFSIZE{DEFAULT_BUFSIZE}
        {
            if(pipe(_pipe.data())) throw std::runtime_error("Unable to open pipes."); 
            fcntl(_pipe[0], F_SETFL, fcntl(_pipe[0], F_GETFL) | O_NONBLOCK);
            fcntl(_pipe[1], F_SETFL, fcntl(_pipe[1], F_GETFL) | O_NONBLOCK);
            if(_which & std::ios_base::out) {
                _write.resize(BUFSIZE);
                Base::setp(_write.data(), _write.data() + _write.size()); 
            }
            if(_which & std::ios_base::in) {
                _read.resize(BUFSIZE);
                Base::setg(_read.data(), _read.data(), _read.data());
            }
        }
        
        pipebuf& pipebuf::operator=(pipebuf&& other){
            _which = std::move(other._which);
            _read = std::move(other._read);
            _write = std::move(other._write);
            _pipe = std::move(other._pipe);
            BUFSIZE = std::move(other.BUFSIZE);
            other._pipe = {};
            Base::operator=(std::move(other));
            return *this;
        }
        
        void pipebuf::close_read() { 
            close(_pipe[0]);
            _read = buffer();
            Base::setg(nullptr, nullptr, nullptr);
            _which &= ~std::ios_base::in;
        }
        
        void pipebuf::close_write() {
            close(_pipe[1]);
            _write = buffer();
            Base::setp(nullptr, nullptr);
            _which &= ~std::ios_base::out;
        }
        
        std::size_t pipebuf::write_remaining() {
            if(Base::pbase() == nullptr) return 0;
            return Base::pptr() - Base::pbase();
        }
        
        pipebuf::~pipebuf(){
            for(int fd: _pipe){
                if(fd > 2) close(fd);
            }
        }
        
        void pipebuf::_resizewbuf(){
            std::size_t off = Base::pptr() - Base::pbase();
            if(off < BUFSIZE-1 && _write.size() > BUFSIZE){
                _write.resize(BUFSIZE);
                _write.shrink_to_fit();
                Base::setp(_write.data(), _write.data() + _write.size());
                Base::pbump(off);
            } else if(Base::pptr() == Base::epptr()) {
                _write.resize(2*(_write.size()));
                Base::setp(_write.data(), _write.data() + _write.size());
                Base::pbump(off);
            }
        }
        
        int pipebuf::_send(pipebuf::char_type *buf, std::size_t size){
            int wfd = _pipe[1];
            std::streamsize len = write(wfd, buf, size);
            while(len >= 0){
                if(static_cast<std::size_t>(len) < size){
                    size -= len;
                    buf += len;
                    len = write(wfd, buf, size);
                } else break;
            }
            if(len < 0){
                switch(errno){
                    case EINTR:
                        return _send(buf, size);
                    case EAGAIN:
                        std::memmove(Base::pbase(), buf, size);
                        Base::setp(Base::pbase(), Base::epptr());
                        pbump(size);
                        return 0;
                    default:
                        return -1;
                }
            }
            Base::setp(Base::pbase(), Base::epptr());
            return 0;
        }
        
        void pipebuf::_mvrbuf() {
            auto garea = Base::egptr() - Base::gptr();
            auto oldarea = Base::gptr() - Base::eback();
            if(garea > 0){
                if(garea < oldarea){
                    std::memcpy(Base::eback(), Base::gptr(), garea);
                } else {
                    std::memmove(Base::eback(), Base::gptr(), garea);
                }
            }
            Base::setg(Base::eback(), Base::eback(), Base::eback()+garea);
        }
        
        int pipebuf::_recv(){
            auto rfd = _pipe[0];
            std::size_t size = Base::eback() + BUFSIZE - Base::egptr();
            std::streamsize len = read(rfd, Base::egptr(), size);
            while(len < 0){
                switch(errno){
                    case EINTR:
                        len = read(rfd, Base::egptr(), size);
                        break;
                    case EAGAIN:
                        return 0;
                    default:
                        return -1;
                }
            }
            if(len == 0){
                return -1;
            }
            Base::setg(Base::eback(), Base::gptr(), Base::egptr()+len);
            return 0;
        }
        
        int pipebuf::sync(){
            if(_which & std::ios_base::out){
                std::size_t size = Base::pptr()-Base::pbase();
                if(size > 0){
                    if(_send(Base::pbase(), size)) return -1;
                }
                _resizewbuf();
            } else if(_which & std::ios_base::in){
                if(Base::gptr() != Base::eback()) _mvrbuf();
                if(_recv()) return -1;
            }
            return 0;
        }
        
        std::streamsize pipebuf::showmanyc() {
            auto which_ = _which;
            _which &= ~std::ios_base::out;
            if(sync()) {
                _which = which_;
                return -1;
            }
            _which = which_;
            return Base::egptr() - Base::gptr();
        }
        
        pipebuf::int_type pipebuf::underflow() {
            if(Base::eback() == nullptr) return traits::eof();
            if(sync()) return traits::eof();
            if(Base::gptr() == Base::egptr()) {
                if(_poll(native_handle(), POLLIN)) return traits::eof();
                return underflow();
            }
            return traits::to_int_type(*Base::gptr());
        }   
        
        pipebuf::int_type pipebuf::overflow(int_type ch) {
            if(Base::pbase() == nullptr) return traits::eof();
            if(sync()) return traits::eof();
            if(Base::pptr() == Base::epptr()){
                if(_poll(native_handle(), POLLOUT)) return traits::eof();
                return overflow(ch);
            }
            if(traits::eq_int_type(ch, traits::eof())) return traits::eof();
            return Base::sputc(ch);
        }
    }
}
