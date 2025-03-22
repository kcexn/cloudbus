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
#include "../io.hpp"
#include "../config.hpp"
#include <csignal>
#include <sys/un.h>
#include <unistd.h>
#pragma once
#ifndef CLOUDBUS_NODE
#define CLOUDBUS_NODE
namespace cloudbus{
    class node_base: public handler_type
    {
        public:
            using Base = handler_type;
            using duration_type = trigger_type::duration_type;
            
            node_base(const duration_type& timeout = duration_type(-1));
            duration_type& timeout() { return _timeout; }
            int run(int notify_pipe=0) { return _run(notify_pipe); }
            int signal_handler(int signal){ return _signal_handler(signal); }
            virtual ~node_base() = default;

            node_base(node_base&& other) = delete;
            node_base(const node_base& other) = delete;
            node_base& operator=(node_base&& other) = delete;
            node_base& operator=(const node_base& other) = delete;    

        protected:
            virtual int _run(int notify_pipe);
            virtual int _signal_handler(int signal){ return 0; }

        private:
            trigger_type _triggers;
            duration_type _timeout;
    };
    template<class ConnectorT>
    class basic_node: public node_base
    {
        public:
            using Base = node_base;
            using connector_type = ConnectorT;

            basic_node(const config::configuration::section& section):
                _connector(triggers(), section){}

            connector_type& connector() { return _connector; }

            virtual ~basic_node() {
                for(auto& n: _connector.north())
                    for(const auto&[addr, addrlen]: n->addresses())
                        if(addr.ss_family == AF_UNIX)
                            unlink(reinterpret_cast<const struct sockaddr_un*>(&addr)->sun_path);
            }

            basic_node() = delete;
            basic_node(basic_node&& other) = delete;
            basic_node(const basic_node& other) = delete;
            basic_node& operator=(basic_node&& other) = delete;
            basic_node& operator=(const basic_node& other) = delete;
        
        protected:
            virtual size_type _handle(events_type& events) override { 
                return _connector.handle(events);
            }
            virtual int _signal_handler(int signal) override {
                if(signal & (SIGTERM | SIGHUP)){
                    if(connector().connections().empty())
                        return signal & (SIGTERM | SIGHUP);
                    timeout() = duration_type(50);
                    connector().drain() = 1;
                }
                return Base::_signal_handler(signal);
            }

        private:
            connector_type _connector;
    };
}
#endif