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
#include "dns.hpp"
#include <mutex>
#include <cstring>
#include <unistd.h>
namespace cloudbus {
    namespace dns {
        extern "C" {
            static void ares_socket_cb(void *data, ares_socket_t socket_fd, int readable, int writable){
                auto *hnd = static_cast<resolver_base::socket_handle*>(data);
                auto&[sockfd, sockstate] = *hnd;
                //if readable is 0 and writable is 0 then socket_fd is closed.
                sockfd = socket_fd;
                sockstate = 0;
                if(readable)
                    sockstate |= resolver_base::READABLE;
                if(writable)
                    sockstate |= resolver_base::WRITABLE;
            }
            static void ares_addrinfo_cb(void *arg, int status, int timeouts, struct ares_addrinfo *result){
                using argument_type = std::tuple<interface_base&, struct ares_addrinfo_hints*>;
                auto *args = static_cast<argument_type*>(arg);
                auto&[interface, hint_ptr] = *args;
                delete hint_ptr;
                int ttl = -1;
                interface_base::addresses_type addresses;
                switch(status){
                    case ARES_SUCCESS:
                        for(auto *node=result->nodes; node != nullptr; node=node->ai_next){
                            auto&[addr, addrlen] = addresses.emplace_back();
                            addrlen = node->ai_addrlen;
                            std::memcpy(&addr, node->ai_addr, addrlen);
                            ttl = (ttl < 0) ? node->ai_ttl : std::min(ttl, node->ai_ttl);
                        }
                        interface.addresses(std::move(addresses), interface_base::duration_type(ttl));
                        ares_freeaddrinfo(result);
                        break;
                    default:
                        break;
                }
                delete args;
            }
        }
        static std::mutex ares_library_mtx;
        static int ares_version_number=0, ares_initialized=0;
        static void initialize_ares_library(){
            const std::lock_guard<std::mutex> lock(ares_library_mtx);
            if( !(ares_initialized++) ){
                const char *v = ares_version(&ares_version_number);
                if(ares_version_number < 0x011201){
                    std::string what = "c-ares version " + std::string(v)
                                    + " is unsupported. Requires "
                                    + "c-ares version >= 1.18.1.";
                    throw std::runtime_error(what);
                }
                if(int err = ares_library_init(ARES_LIB_INIT_ALL)){
                    std::string what = "c-ares failed to initialize with error: "
                                    + std::string(ares_strerror(err));
                    throw std::runtime_error(what);
                }
            }
        }
        static void cleanup_ares_library(){
            const std::lock_guard<std::mutex> lock(ares_library_mtx);
            if( !(--ares_initialized) )
                ares_library_cleanup();
        }
        static void initialize_ares_channel(ares_channel *channel, ares_options *opts, int mask){
            int status = ares_init_options(
                channel,
                opts,
                mask
            );
            switch(status){
                case ARES_ENOTINITIALIZED:
                    throw std::runtime_error("The c-ares library is not initialized.");
                case ARES_ENOMEM:
                    throw std::bad_alloc();
                case ARES_EFILE:
                    throw std::runtime_error(
                        "c-ares channel failed to initialize "
                        "because a DNS configuration file could "
                        "not be read."
                    );
                default:
                    return;
            }
        }
        resolver_base::resolver_base():
            _handle{ARES_SOCKET_BAD, 0}, _timeout{clock_type::now(), -1},
            _channel{}, _opts{}
        {
            initialize_ares_library();
            _opts.timeout = 500;
            _opts.sock_state_cb = ares_socket_cb;
            _opts.sock_state_cb_data = &_handle;
            initialize_ares_channel(&_channel, &_opts, ARES_OPT_SOCK_STATE_CB | ARES_OPT_TIMEOUTMS | ARES_OPT_ROTATE);
        }
        resolver_base::duration_type resolver_base::resolve(interface_base& interface){
            using argument_type = std::tuple<interface_base&, struct ares_addrinfo_hints*>;
            struct ares_addrinfo_hints *hints = new struct ares_addrinfo_hints;
            std::memset(hints, 0, sizeof(struct ares_addrinfo_hints));
            hints->ai_family = AF_INET;
            if(interface.protocol() == "TCP")
                hints->ai_socktype = SOCK_STREAM;
            argument_type *args = new argument_type{interface, hints};
            auto delim = interface.uri().find(':');
            ares_getaddrinfo(
                _channel,
                interface.uri().substr(0, delim).c_str(),
                interface.uri().substr(delim+1).c_str(),
                hints,
                ares_addrinfo_cb,
                args
            );
            return set_timeout();
        }
        resolver_base::duration_type resolver_base::process_event(){
            ares_socket_t readfd=ARES_SOCKET_BAD, writefd=ARES_SOCKET_BAD;
            auto&[sockfd, sockev] = _handle;
            auto&[time, interval] = _timeout;
            if(clock_type::now() < time+interval){
                if(sockev & READABLE)
                    readfd = sockfd;
                if(sockev & WRITABLE)
                    writefd = sockfd;
            }
            ares_process_fd(_channel, readfd, writefd);
            return set_timeout();
        }
        resolver_base::~resolver_base(){
            ares_destroy(_channel);
            cleanup_ares_library();
        }
        resolver_base::duration_type resolver_base::set_timeout(){
            struct timeval tv = {};
            if( ares_timeout(_channel, nullptr, &tv) ){
                auto interval = duration_type(tv.tv_sec*1000+tv.tv_usec/1000);
                _timeout = timeout_type{clock_type::now(), interval};
                return interval;
            }
            _timeout = timeout_type{clock_type::now(), -1};
            return duration_type(-1);
        }
    }
}