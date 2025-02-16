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
namespace cloudbus{
    namespace proxy{
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
            std::getline(f, line);
            if(registry::make_address(line, reinterpret_cast<struct sockaddr*>(&ss), &len, protocol)) throw std::runtime_error("Invalid configuration.");
            connector().make(connector().south(), reinterpret_cast<const connector_type::south_type::address_type>(&ss), len);
        }
        proxy::~proxy() {
            for(auto& n: connector().north()){
                if(n->address()->sa_family == AF_UNIX)
                    unlink(reinterpret_cast<struct sockaddr_un*>(n->address())->sun_path);
            }
        }

        volatile static std::sig_atomic_t signal = 0;
        extern "C" {
            static void sighandler(int sig){
                signal = sig;
            }
        }
        int proxy::run(){
            constexpr int FAIRNESS = 16;
            auto pause = std::chrono::milliseconds(-1);
            std::signal(SIGTERM, sighandler);
            std::signal(SIGINT, sighandler);
            std::signal(SIGHUP, sighandler);
            while(triggers().wait(pause) >= 0){
                auto events = triggers().events();
                for(int i = 0, handled = handle(events); handled != 0 && i++ < FAIRNESS; handled = handle(events));
                if(signal) return signal;
            }
            return 0;
        }
        int proxy::_handle(events_type& events){
            return connector().handle(events);
        }
    }
}