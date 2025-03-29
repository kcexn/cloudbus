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
#include "manager.hpp"
#include <thread>
#include <fcntl.h>
namespace cloudbus {
    static void set_flags(int handle){
        if(fcntl(handle, F_SETFD, FD_CLOEXEC))
            throw std::runtime_error("Unable to set CLOEXEC flag.");
    }
    volatile std::sig_atomic_t manager_base::signal = 0;
    extern "C" {
        static void sighandler(int sig){
            manager_base::signal = sig;
        }
    }
    static void unmask_handlers(){
        std::signal(SIGTERM, sighandler);
        std::signal(SIGHUP, sighandler);
        std::signal(SIGINT, sighandler);
    }
    static void mask_handlers(){
        std::signal(SIGTERM, SIG_IGN);
        std::signal(SIGHUP, SIG_IGN);
        std::signal(SIGINT, SIG_IGN);
    }

    manager_base::manager_base(const config_type& config):
        _config{config}
    { unmask_handlers(); }

    void manager_base::start(node_type& node){
        pipe_type p;
        if(pipe(p.data()))
            throw std::runtime_error("Unable to open pipe.");
        for(auto& hnd: p)
            set_flags(hnd);
        mask_handlers();
        _threads.emplace_back(p, std::thread([](node_type& n, int noticefd){
            return n.run(noticefd);
        }, std::ref(node), p[0]));
        unmask_handlers();
    }

    void manager_base::join(std::size_t start, std::size_t size){
        auto it = _threads.begin();
        while(start-- && it != _threads.end())
            ++it;
        while(size-- && it != _threads.end()){
            auto&[p, t] = *it;
            t.join();
            for(auto hnd: p)
                close(hnd);
            it = _threads.erase(it);
        }
    }

    int manager_base::handle_signal(int sig) {
        if(sig){
            for(auto&[p, t]: _threads){
                int wr=p[1], off=0;
                char *buf = reinterpret_cast<char*>(&sig);
                while(auto len = write(wr, buf+off, sizeof(sig)-off)){
                    if(len < 0){
                        switch(errno){
                            case EINTR: continue;
                            default:
                                throw std::runtime_error("Couldn't notify thread of signal.");
                        }
                    }
                    if((off+=len) == sizeof(sig))
                        break;
                }
            }
        }
        return _handle_signal(sig);
    }

    int manager_base::_handle_signal(int sig) {
        if(sig & (SIGTERM | SIGINT | SIGHUP)){
            join();
            sig &= ~(SIGTERM | SIGINT | SIGHUP);
        }
        return sig;
    }

    int manager_base::_run(){
        while(pause())
            if(auto mask=handle_signal(signal); signal & ~mask)
                return signal & ~mask;
        return 0;
    }
}