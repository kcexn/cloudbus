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
#include "../registry.hpp"
#include "controller.hpp"
#include <algorithm>
#include <fstream>
#include <csignal>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#ifdef PROFILE
    #include <chrono>
    #include <iostream>
#endif
namespace cloudbus{
    namespace controller{
        static int set_flags(int fd){
            int flags = 0;
            if(fcntl(fd, F_SETFD, FD_CLOEXEC))
                throw std::runtime_error("Unable to set the cloexec flag.");
            if((flags = fcntl(fd, F_GETFL)) == -1)
                throw std::runtime_error("Unable to get file flags.");
            if(fcntl(fd, F_SETFL, flags | O_NONBLOCK))
                throw std::runtime_error("Unable to set the socket to nonblocking mode.");
            return fd;
        }
        static int _accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen){
            int fd =0;
            if((fd = accept(sockfd, addr, addrlen)) >= 0){
                return set_flags(fd);
            } else {
                switch(errno){
                    case EINTR:
                        return _accept(sockfd, addr, addrlen);
                    case EWOULDBLOCK:
//                  case EAGAIN:
                        return -1;
                    default:
                        throw std::runtime_error("Unable to accept connected socket.");
                }
            }
        }
        
        volatile static std::sig_atomic_t signal = 0;
        extern "C" {
            static void sighandler(int sig){
                signal = sig;
            }
        }
        static controller_base::events_type filter_events(const controller_base::events_type& events, const controller_base::event_mask& mask=-1){
            auto e_ = controller_base::events_type();
            e_.reserve(events.size());
            for(const auto& e: events)
                if(e.revents & mask)
                    e_.push_back(e);
            return e_;
        }
        int controller_base::_run() {
            constexpr size_type FAIRNESS = 16;
            const auto pause = std::chrono::milliseconds(-1);
            std::signal(SIGTERM, sighandler);
            std::signal(SIGINT, sighandler);
            std::signal(SIGHUP, sighandler);
            while(triggers().wait(pause) != trigger_type::npos){
                auto events = triggers().events();
                for(size_type i = 0, handled = handle(events); i++ < FAIRNESS && !handled; handled = handle(events)){
                    if(handled == trigger_type::npos)
                        goto EXIT;
                    if(i == FAIRNESS){
                        if((i = triggers().wait(std::chrono::milliseconds(0)))){
                            if(i == trigger_type::npos)
                                goto EXIT;
                            events = filter_events(events);
                            auto events_ = triggers().events();
                            for(auto e = events_.begin(); i && e < events_.end(); ++e){
                                if(e->revents && i--){
                                    auto it = std::find_if(events.begin(), events.end(), [&](auto& ev){ return e->fd == ev.fd; });
                                    if(it != events.end())
                                        it->revents |= e->revents;
                                    else events.push_back(*it);
                                }
                            }
                        }
                        if(signal) return signal;
                    }
                }
                if(signal) return signal;
            }
        EXIT:
            if(signal) return signal;
            return 0;
        }
        controller::controller(): Base(){
            struct sockaddr_storage ss = {};
            socklen_t len = 0;
            std::string filename{"controller.ini"};
            std::fstream f(filename, f.in);
            if(!f.is_open()) throw std::runtime_error("Unable to open controller.ini");
            std::string line;
            registry::transport protocol;
            std::getline(f, line);
            if(registry::make_address(line, reinterpret_cast<struct sockaddr*>(&ss), &len, protocol)) throw std::runtime_error("Invalid configuration.");
            connector().make(connector().north(), reinterpret_cast<const struct sockaddr*>(&ss), len);
            
            line.clear();
            while(std::getline(f, line)){
                if(registry::make_address(line, reinterpret_cast<struct sockaddr*>(&ss), &len, protocol))
                    throw std::runtime_error("Invalid configuration.");
                connector().make(connector().south(), reinterpret_cast<const struct sockaddr*>(&ss), len);
                line.clear();
            }
            std::string mode;
            if(auto *m = std::getenv("CLOUDBUS_SERVICE_MODE"); m != nullptr)
                mode=std::string(m);
            std::transform(mode.begin(), mode.end(), mode.begin(), [](const unsigned char c){ return std::toupper(c); });
            if(!mode.empty() && mode=="FULL_DUPLEX"){
                if(connector().south().size() > messages::CLOCK_SEQ_MAX)
                    throw std::runtime_error("The service fanout ratio will overflow the UUID clock_seq.");
                connector().mode() = connector_type::FULL_DUPLEX;
            } else connector().mode() = connector_type::HALF_DUPLEX;
        }
        controller::~controller(){
            for(auto& n: connector().north())
                for(const auto&[addr, addrlen]: n->addresses())
                    if(addr.ss_family == AF_UNIX)
                        unlink(reinterpret_cast<const struct sockaddr_un*>(&addr)->sun_path);
        }
        controller::size_type controller::_handle(events_type& events){
            return connector().handle(events);
        }
    }
}