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
#include <algorithm>
#include <csignal>
#include <unistd.h>
namespace cloudbus{
    node_base::node_base(const duration_type& timeout):
        _triggers{}, _timeout{timeout}{}

    volatile static std::sig_atomic_t signal = 0;
    extern "C" {
        static void sighandler(int sig){
            signal = sig;
        }
    }
    static ssize_t read_notice(const int& notify_pipe, int& notice){
        int len = 0, notice_ = 0;
        if((len = read(notify_pipe, &notice_, sizeof(notice_))) < 0){
            switch(errno){
                case EINTR: return read_notice(notify_pipe, notice);
                default: break;
            }
        }
        notice |= notice_;
        return len;
    }
    static int check_for_signal(
        const node_base::events_type& events,
        node_base::size_type& n,
        const int& notify_pipe,
        int& notice
    ){
        ssize_t len = 0;
        auto cit = std::find_if(events.cbegin(), events.cend(), [&notify_pipe](const auto& ev){ return ev.fd == notify_pipe; });
        if(cit != events.cend() && cit->revents && n--)
            if(cit->revents & (POLLERR | POLLNVAL)
                    || (len = read_notice(notify_pipe, notice)) <= 0)
                return -1;
        return 0;
    }
    static node_base::size_type filter_events(node_base::events_type& events, const node_base::event_mask& mask=-1){
        auto put = events.begin();
        for(const auto& e: events)
            if(e.revents & mask)
                *(put++) = e;
        events.resize(put - events.begin());
        return events.size();
    }
    int node_base::_run(int notify_pipe) {
        constexpr size_type FAIRNESS = 16;
        size_type n = 0;
        int notice = 0;
        if(!notify_pipe){
            std::signal(SIGTERM, sighandler);
            std::signal(SIGHUP, sighandler);
        } else triggers().set(notify_pipe, POLLIN);
        while((n = triggers().wait(_timeout)) != trigger_type::npos){
            auto events = triggers().events();
            if(notify_pipe && check_for_signal(events, n, notify_pipe, notice))
                return notice;
            for(size_type i = 0, handled = handle(events); n && handled; handled = handle(events)){
                if(handled == trigger_type::npos)
                    return notify_pipe ? notice : signal;
                if(++i == FAIRNESS){
                    n = filter_events(events);
                    if((i = triggers().wait()) != trigger_type::npos){
                        for(const auto& e: triggers().events()){
                            if(e.revents && i--){
                                auto it = std::find_if(events.begin(), events.end(), [&](auto& ev){
                                    if(e.fd==ev.fd)
                                        ev.revents |= e.revents;
                                    return e.fd==ev.fd;
                                });
                                if(it == events.end() && ++n)
                                    events.push_back(e);
                            }
                            if(!i) break;
                        }
                    } else return notify_pipe ? notice : signal;
                    if(notify_pipe && check_for_signal(events, n, notify_pipe, notice))
                        return notice;
                    if(auto status = notify_pipe ? signal_handler(notice) : signal_handler(signal))
                        return status;
                }
            }
            if(auto status = notify_pipe ? signal_handler(notice) : signal_handler(signal))
                return status;
        }
        return notify_pipe ? notice : signal;
    }
}