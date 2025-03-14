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
#include "proxy.hpp"
#include <string>
#include <fstream>
#include <csignal>
#include <sys/socket.h>
#include <sys/un.h>
#ifdef PROFILE
    #include <chrono>
    #include <iostream>
#endif
namespace cloudbus{
    namespace proxy{
        static proxy_base::events_type filter_events(const proxy::events_type& events, const proxy::event_mask& mask=-1){
            auto e_ = proxy_base::events_type();
            e_.reserve(events.size());
            for(const auto& e: events)
                if(e.revents & mask)
                    e_.push_back(e);
            return e_;
        }
        volatile static std::sig_atomic_t signal = 0;
        extern "C" {
            static void sighandler(int sig){
                signal = sig;
            }
        }        
        int proxy_base::_run(){
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
        proxy::proxy(): Base(){
            std::string filename{"proxy.ini"};
            std::fstream f(filename, f.in);
            if(!f.is_open()) throw std::runtime_error("Unable to open proxy.ini");
            std::string line{};
            std::getline(f, line);
            if(auto nfd = connector().make_north(registry::make_address(line)); nfd >= 0)
                triggers().set(nfd, POLLIN);
            else throw std::invalid_argument("Invalid configuration.");
            line.clear();
            while(std::getline(f, line)){
                if(connector().make_south(registry::make_address(line)))
                    throw std::invalid_argument("Invalid configuration.");
                line.clear();
            }
            std::string mode{};
            if(auto *m = std::getenv("CLOUDBUS_SERVICE_MODE"); m != nullptr)
                mode=std::string(m);
            std::transform(mode.begin(), mode.end(), mode.begin(), [](const unsigned char c){ return std::toupper(c); });
            if(!mode.empty() && mode=="FULL_DUPLEX"){
                if(connector().south().size() > messages::CLOCK_SEQ_MAX)
                    throw std::runtime_error("The service fanout ratio will overflow the UUID clock_seq.");
                connector().mode() = connector_type::FULL_DUPLEX;
            } else connector().mode() = connector_type::HALF_DUPLEX;
        }
        proxy::~proxy() {
            for(auto& n: connector().north())
                for(const auto&[addr, addrlen]: n->addresses())
                    if(addr.ss_family == AF_UNIX)
                        unlink(reinterpret_cast<const struct sockaddr_un*>(&addr)->sun_path);
        }
        proxy::size_type proxy::_handle(events_type& events){
            return connector().handle(events);
        }
    }
}