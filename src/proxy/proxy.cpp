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
                        return 0;
                    if(i == FAIRNESS){
                        if((i = triggers().wait(std::chrono::milliseconds(0)))){
                            if(i == trigger_type::npos) return 0;
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
            }
            return 0;
        }
        proxy::proxy(): Base(){
            struct sockaddr_storage ss = {};
            socklen_t len = 0;
            std::string filename{"proxy.ini"};
            std::fstream f(filename, f.in);
            if(!f.is_open()) throw std::runtime_error("Unable to open proxy.ini");
            std::string line;
            registry::transport protocol;
            std::getline(f, line);
            if(registry::make_address(line, reinterpret_cast<struct sockaddr*>(&ss), &len, protocol)) throw std::runtime_error("Invalid configuration.");
            connector().make(connector().north(), reinterpret_cast<const connector_type::north_type::address_type>(&ss), len);

            line.clear();
            while(std::getline(f, line)){
                if(registry::make_address(line, reinterpret_cast<struct sockaddr*>(&ss), &len, protocol)) 
                    throw std::runtime_error("Invalid configuration.");
                connector().make(connector().south(), reinterpret_cast<const connector_type::south_type::address_type>(&ss), len);
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
        proxy::~proxy() {
            for(auto& n: connector().north()){
                if(n->address()->sa_family == AF_UNIX)
                    unlink(reinterpret_cast<struct sockaddr_un*>(n->address())->sun_path);
            }
        }
        proxy::size_type proxy::_handle(events_type& events){
            return connector().handle(events);
        }
    }
}