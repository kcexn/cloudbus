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
#include <unistd.h>
namespace cloudbus{
    node_base::node_base():
        Base(_triggers), _triggers{}, _timeout{-1}
    {}

    volatile static std::sig_atomic_t signal = 0;
    extern "C" {
        static void sighandler(int sig){
            signal = sig;
        }
    }
    static int read_notice(const int& notify_pipe, int& notice){
        int notice_=0, off=0;
        char *buf = reinterpret_cast<char*>(&notice_);
        while(int len = read(notify_pipe, buf+off, sizeof(notice_)-off)){
            if(len < 0){
                switch(errno){
                    case EINTR: continue;
                    default: return len;
                }
            }
            if((off+=len) == sizeof(notice_))
                break;
        }
        notice |= notice_;
        return 0;
    }
    static int check_for_signal(
        node_base::events_type& events,
        const int& notify_pipe,
        int& notice
    ){
        if(!notify_pipe){
            if(notice |= signal)
                signal = 0;
            return 0;
        }
        auto it = std::find_if(
                events.begin(),
                events.end(),
            [&notify_pipe](auto& event){
                return event.fd==notify_pipe;
            }
        );
        if(it != events.end()){
            if(it->revents & (POLLERR | POLLNVAL))
                return -1;
            auto revents = it->revents;
            it = events.erase(it);
            if(revents)
                return read_notice(notify_pipe, notice);
        }
        return 0;
    }
    int node_base::_run(int notify_pipe) {
        constexpr size_type FAIRNESS = 16;
        size_type n = 0;
        int notice = 0;
        if(!notify_pipe){
            std::signal(SIGTERM, sighandler);
            std::signal(SIGINT, sighandler);
            std::signal(SIGHUP, sighandler);
        } else triggers().set(notify_pipe, POLLIN);
        while( (n = triggers().wait(_timeout)) != trigger_type::npos ){
            auto events = n ? triggers().events() : events_type();
            if(check_for_signal(events, notify_pipe, notice))
                return notice;
            for(size_type i=0, handled=handle(events); handled; handled=handle(events)){
                if(handled == trigger_type::npos)
                    return notice;
                if(++i == FAIRNESS){
                    if( (i = triggers().wait()) != trigger_type::npos ){
                        for(const auto& e: triggers().events()){
                            if(e.revents && i--){
                                auto it = std::find_if(
                                        events.begin(),
                                        events.end(),
                                    [&](auto& ev){
                                        if(e.fd==ev.fd)
                                            ev.revents |= e.revents;
                                        return e.fd==ev.fd;
                                    }
                                );
                                if(it == events.end())
                                    events.push_back(e);
                            }
                            if(!i) break;
                        }
                    } else return notice;
                    if(check_for_signal(events, notify_pipe, notice))
                        return notice;
                    if(auto mask=signal_handler(notice); notice & ~mask)
                        return notice & ~mask;
                }
            }
            if(auto mask=signal_handler(notice); notice & ~mask)
                return notice & ~mask;
        }
        return notice;
    }
}