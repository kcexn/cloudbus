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
#include "node.hpp"
#include <csignal>
namespace cloudbus{
    volatile static std::sig_atomic_t signal = 0;
    extern "C" {
        static void sighandler(int sig){
            signal = sig;
        }
    }
    static int clean_exit(){
        if(signal)
            return signal;
        return 0;
    }
    static node_base::events_type filter_events(const node_base::events_type& events, const node_base::event_mask& mask=-1){
        auto e_ = node_base::events_type();
        e_.reserve(events.size());
        for(const auto& e: events)
            if(e.revents & mask)
                e_.push_back(e);
        return e_;
    }
    int node_base::_run() {
        constexpr size_type FAIRNESS = 16;
        const auto pause = std::chrono::milliseconds(-1);
        std::signal(SIGTERM, sighandler);
        std::signal(SIGINT, sighandler);
        std::signal(SIGHUP, sighandler);
        while(triggers().wait(pause) != trigger_type::npos){
            auto events = triggers().events();
            for(size_type i = 0, handled = handle(events); i++ < FAIRNESS && handled; handled = handle(events)){
                if(handled == trigger_type::npos)
                    return clean_exit();
                if(i == FAIRNESS){
                    if((i = triggers().wait(std::chrono::milliseconds(0)))){
                        if(i == trigger_type::npos)
                            return clean_exit();
                        events = filter_events(events);
                        auto events_ = triggers().events();
                        for(auto e = events_.cbegin(); i && e < events_.cend(); ++e){
                            if(e->revents && i--){
                                auto it = std::find_if(events.begin(), events.end(), [&](const auto& ev){ return e->fd == ev.fd; });
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
        return clean_exit();
    }
}
