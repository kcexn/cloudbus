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
    }
    static void mask_handlers(){
        std::signal(SIGTERM, SIG_IGN);
        std::signal(SIGHUP, SIG_IGN);
    }

    manager_base::manager_base(const config_type& config):
        _config{config}
    { unmask_handlers(); }

    manager_base::pipe_type manager_base::start(node_type& node){
        pipe_type p;
        if(pipe(p.data()))
            throw std::runtime_error("Unable to open pipe.");
        for(auto& hnd: p)
            set_flags(hnd);
        mask_handlers();
        _threads.emplace_back([](node_type& n, int noticefd){
            return n.run(noticefd);
        }, std::ref(node), p[0]);
        unmask_handlers();
        return p;
    }

    void manager_base::join(){
        for(auto& t: _threads)
            t.join();
    }
}