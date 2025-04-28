/*
*   Copyright 2025 Kevin Exton
*   This file is part of Cloudbus.
*
*   Cloudbus is free software: you can redistribute it and/or modify it under the
*   terms of the GNU General Public License as published by the Free Software
*   Foundation, either version 3 of the License, or any later version.
*
*   Cloudbus is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
*   without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*   See the GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License along with Cloudbus.
*   If not, see <https://www.gnu.org/licenses/>.
*/
#include "manager.hpp"
#include <thread>
#include <fcntl.h>
#include <ctime>
namespace cloudbus {
    static void set_flags(int handle){
        if(fcntl(handle, F_SETFD, FD_CLOEXEC))
            throw std::runtime_error("Unable to set CLOEXEC flag.");
    }  
    volatile std::sig_atomic_t manager_base::sigterm=0, manager_base::sighup=0, manager_base::sigint=0, manager_base::sigusr1=0;
    extern "C" {
        static void sighandler(int sig) {
            switch(sig) {
                case SIGTERM:
                    return (void)(manager_base::sigterm = sig);
                case SIGINT:
                    return (void)(manager_base::sigint = sig);
                case SIGHUP:
                    return (void)(manager_base::sighup = sig);
                case SIGUSR1:
                    return (void)(manager_base::sigusr1 = sig);
                default:
                    break;
            }
        }
    }
    static void unmask_handlers() {
        std::signal(SIGTERM, sighandler);
        std::signal(SIGHUP, sighandler);
        std::signal(SIGINT, sighandler);
        std::signal(SIGUSR1, sighandler);
    }
    static void mask_handlers() {
        std::signal(SIGTERM, SIG_IGN);
        std::signal(SIGHUP, SIG_IGN);
        std::signal(SIGINT, SIG_IGN);
        std::signal(SIGUSR1, SIG_IGN);
    }
    manager_base::manager_base(const config_type& config):
        _config{config}
    { unmask_handlers(); }
    void manager_base::start(const std::string& name, node_type& node) {
        pipe_type p{};
        if(pipe(p.data()))
            throw std::runtime_error("Unable to open pipe.");
        for(const auto& hnd: p)
            set_flags(hnd);
        mask_handlers();
        _threads.try_emplace(name, p, std::thread([](node_type& n, int noticefd){
            return n.run(noticefd);
        }, std::ref(node), p[0]));
        unmask_handlers();
    }
    void manager_base::join(threads_type::iterator it) {
        if(it != _threads.end()) {
            auto&[name, thrd] = *it;
            auto&[p, t] = thrd;
            t.join();
            for(auto hnd: p)
                close(hnd);
        }
    }
    static int notify_thread(int pfd, int sig) {
        int off = 0;
        char *buf = reinterpret_cast<char*>(&sig);
        while(auto len = write(pfd, buf+off, sizeof(sig)-off)) {
            if(len < 0) {
                switch(errno) {
                    case EINTR:
                        continue;
                    default:
                        return -errno;
                }
            }
            if((off+=len) == sizeof(sig))
                break;
        }
        return 0;
    }
    manager_base::threads_type::iterator manager_base::stop(threads_type::iterator it) {
        if(it == _threads.end())
            return it;
        auto&[name, thrd] = *it;
        auto&[p, t] = thrd;
        if(int ec = notify_thread(p[1], SIGTERM)) {
            throw std::system_error (
                std::error_code(-ec, std::system_category()),
                "Unable to terminate thread."
            );
        }
        join(it);
        return _threads.erase(it);
    }
    int manager_base::handle_signal(int sig) {
        return _handle_signal(sig);
    }
    int manager_base::_handle_signal(int sig) {
        if(sig == SIGTERM || sig == SIGINT || sig == SIGHUP) {
            auto it = _threads.begin();
            while( (it = stop(it)) != _threads.end() );
            return 0;
        }
        return sig;
    }

    int manager_base::_run() {
        int rc = 0;
        struct timespec ts = {30, 0}, tr = ts;
        do{
            if(rc && errno != EINTR)
                return errno;
            if(sighup) {
                sighup = handle_signal(sighup);
                return SIGHUP;
            }
            if(sigint) {
                sigint = handle_signal(sigint);
                return SIGINT;
            }
            if(sigusr1)
                sigusr1 = handle_signal(sigusr1);
            if(sigterm) {
                sigterm = handle_signal(sigterm);
                return SIGTERM;
            }
            ts = tr;
        } while( (rc = nanosleep(&ts, &tr)) );
        return run();
    }
}