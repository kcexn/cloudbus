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
namespace cloudbus {
    namespace dns {
        static const int ares_channel_optmask = 0;
        static const struct ares_options ares_channel_opts = {
            0,          //int flags;
            0,          //int timeout;
            0,          //int tries;
            0,          //int ndots;
            0,          //unsigned short udp_port;
            0,          //unsigned short tcp_port;
            0,          //int socket_send_buffer_size;
            0,          //int socket_receive_buffer_size;
            nullptr,    //struct in_addr *servers;
            0,          //int nservers;
            nullptr,    //char **domains;
            0,          //int ndomains;
            nullptr,    //char *lookups;
            nullptr,    //ares_sock_state_cb sock_state_cb;
            nullptr,    //void *sock_state_cb_data;
            nullptr,    //struct apattern *sortlist;
            0,          //int nsort;
            0,          //int ednspsz;
            nullptr     //char *resolvconf_path;
        };
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
        static void initialize_ares_channel(ares_channel *channel){
            int status = ares_init_options(
                channel,
                const_cast<struct ares_options*>(&ares_channel_opts),
                ares_channel_optmask
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
            _channel{}
        {
            initialize_ares_library();
            initialize_ares_channel(&_channel);
        }
        resolver_base::~resolver_base(){
            ares_destroy(_channel);
            cleanup_ares_library();
        }
    }
}