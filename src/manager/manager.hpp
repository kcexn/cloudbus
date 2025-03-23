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
#include "../node.hpp"
#include "../config.hpp"
#include <thread>
#include <list>
#pragma once
#ifndef CLOUDBUS_MANAGER
#define CLOUDBUS_MANAGER
namespace cloudbus {
    class manager_base{
        public:
            using config_type = config::configuration;
            using node_type = node_base;
            using pipe_type = std::array<int, 2>;
            using thread_type = std::tuple<pipe_type, std::thread>;
            using threads_type = std::list<thread_type>;
            static volatile std::sig_atomic_t signal;

            manager_base(const config::configuration& config);

            const config_type& config() const { return _config; }
            void start(node_type& node);
            int run() { return _run(); }
            int handle_signal(int sig);
            void join(std::size_t start=0, std::size_t size=-1);

            virtual ~manager_base() = default;

            manager_base() = delete;
            manager_base(const manager_base& other) = delete;
            manager_base(manager_base&& other) = delete;
            manager_base& operator=(const manager_base& other) = delete;
            manager_base& operator=(manager_base&& other) = delete;
        
        protected:
            virtual int _handle_signal(int sig);
            virtual int _run();

        private:
            config_type _config;
            threads_type _threads;
    };

    template<class NodeT>
    class basic_manager : public manager_base
    {
        public:
            using Base = manager_base;
            using node_type = NodeT;
            using service_type = std::tuple<std::string, node_type>;
            using services_type = std::list<service_type>;

            basic_manager(const config_type& config):
                Base(config), _services{}
            {
                for(const auto& section: config.sections()){
                    std::string heading = section.heading;
                    std::transform(heading.begin(), heading.end(), heading.begin(), [](unsigned char c){ return std::toupper(c); });
                    if(heading != "CLOUDBUS")
                        _services.emplace_back(section.heading, section);
                }
            }

            services_type& services() { return _services; }
            const services_type& services() const { return _services; }

            virtual ~basic_manager() = default;

            basic_manager() = delete;
            basic_manager(const basic_manager& other) = delete;
            basic_manager(basic_manager&& other) = delete;
            basic_manager& operator=(const basic_manager& other) = delete;
            basic_manager& operator=(basic_manager&& other) = delete;

        protected:
            virtual int _run() override {
                for(auto&[name, node]: services())
                    start(node);
                return Base::_run();
            }

        private:
            services_type _services;
    };
}
#endif