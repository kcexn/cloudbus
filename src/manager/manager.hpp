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
#include "../node.hpp"
#include "../config.hpp"
#include <fstream>
#include <thread>
#include <list>
#include <filesystem>
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
            using threads_type = std::map<std::string, thread_type>;
            static volatile std::sig_atomic_t sigterm, sighup, sigint, sigusr1;

            explicit manager_base(const config::configuration& config);

            const config_type& config() const { return _config; }
            threads_type& threads() { return _threads; }
            void start(const std::string& name, node_type& node);
            int run() { return _run(); }
            int handle_signal(int sig);
            void join(threads_type::iterator it);
            threads_type::iterator stop(threads_type::iterator it);

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
            using services_type = std::map<std::string, node_type>;

            explicit basic_manager(const config_type& config):
                Base(config), _services{}, mtime{}
            {
                merge(config);
                #ifdef CONFDIR
                    std::string path{CONFDIR};
                #else
                    std::string path{"."};
                #endif
                #ifdef COMPILE_CONTROLLER
                    path += "/controller.ini";
                #elif defined(COMPILE_SEGMENT)
                    path += "/segment.ini";
                #endif
                mtime = std::filesystem::last_write_time(path);
            }

            services_type& services() { return _services; }
            const services_type& services() const { return _services; }

            void merge(const config_type& config) {
                auto ity = services().begin();
                while(ity != services().end()) {
                    const auto&[heading, node] = *ity++;
                    auto itx = config.sections().find(heading);
                    if(itx == config.sections().end()) {
                        stop(threads().find(heading));
                        ity = services().erase(--ity);
                    }
                }
                for(auto&[heading, section]: config.sections()) {
                    std::string h = heading;
                    std::transform(h.begin(), h.end(), h.begin(), [](unsigned char c){ return std::tolower(c); });
                    if(h != "cloudbus") {
                        auto[ity, emplaced] = services().try_emplace(heading, section);
                        while(!emplaced) {
                            auto&[head, node] = *ity;
                            if(node.conf() == section)
                                break;
                            stop(threads().find(heading));
                            services().erase(ity);
                            auto[itv, empl] = services().try_emplace(heading, section);
                            ity = itv; emplaced = empl;
                        }
                        if(emplaced) {
                            auto&[head, node] = *ity;
                            start(heading, node);
                        }
                    }
                }
            }

            virtual ~basic_manager() = default;

            basic_manager() = delete;
            basic_manager(const basic_manager& other) = delete;
            basic_manager(basic_manager&& other) = delete;
            basic_manager& operator=(const basic_manager& other) = delete;
            basic_manager& operator=(basic_manager&& other) = delete;

        protected:
            virtual int _run() override {
                #ifdef CONFDIR
                    std::string path{CONFDIR};
                #else
                    std::string path{"."};
                #endif
                #ifdef COMPILE_CONTROLLER
                    path += "/controller.ini";
                #elif defined(COMPILE_SEGMENT)
                    path += "/segment.ini";
                #endif
                auto mt = std::filesystem::last_write_time(path);
                if(mt != mtime) {
                    mtime = mt;
                    std::raise(SIGUSR1);
                }
                return Base::_run();
            }
            virtual int _handle_signal(int sig) override {
                if(sig == SIGUSR1) {
                    #ifdef CONFDIR
                        std::string path{CONFDIR};
                    #else
                        std::string path{"."};
                    #endif
                    #ifdef COMPILE_CONTROLLER
                        path += "/controller.ini";
                    #elif defined(COMPILE_SEGMENT)
                        path += "/segment.ini";
                    #endif
                    config::configuration conf;
                    if(std::fstream f{path, f.in}; f.good()) {
                        f >> conf;
                        merge(conf);
                    }
                    return 0;
                }
                return Base::_handle_signal(sig);
            }

        private:
            services_type _services;
            std::filesystem::file_time_type mtime;
    };
}
#endif
