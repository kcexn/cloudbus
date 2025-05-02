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
#include "../interfaces.hpp"
#include <thread>
#include <map>
#include <mutex>
#pragma once
#ifndef CLOUDBUS_METRICS
#define CLOUDBUS_METRICS
namespace cloudbus {
    class stream_metrics {
        public:
            using stream_type = interface_base::stream_type;
            using clock_type = std::chrono::steady_clock;
            using duration_type = std::chrono::milliseconds;
            using weak_ptr = std::weak_ptr<stream_type>;
            using time_point = clock_type::time_point;

            struct metric_type {
                weak_ptr wp;
                duration_type interarrival, intercompletion;
                time_point last_arrival, last_completion;
            };
            using metrics_vec = std::vector<metric_type>;

            duration_type add_completion(weak_ptr ptr, const time_point& t=clock_type::now());
            duration_type add_arrival(weak_ptr ptr, const time_point& t=clock_type::now());
            const metrics_vec& get_all_measurements() const { return measurements; }
        private:
            metrics_vec measurements;
    };
    class node_metrics {
        public:
            /* carried traffic = offered traffic unless offered traffic *
             * exceeds capacity. I only need to count arrivals if I am  *
             * interested in throughput. I will need to compute capacity*
             * using a different method.                                */
            std::size_t arrivals;
            stream_metrics streams;

            node_metrics() : arrivals{0}{}
            ~node_metrics() = default;
    };
    class metrics {
        public:
            using node_pair = std::pair<std::thread::id, node_metrics>;
            using node_registry = std::map<std::thread::id, node_metrics>;
            using node_metrics_vec = std::vector<node_pair>;
            static inline metrics& get() {
                static metrics m;
                return m;
            }

            node_metrics& make_node(const std::thread::id& tid = std::this_thread::get_id());
            std::size_t& arrivals(const std::thread::id& tid = std::this_thread::get_id());
            stream_metrics& streams(const std::thread::id& tid = std::this_thread::get_id());
            node_metrics_vec get_all_measurements();
            void erase_node(const std::thread::id& tid = std::this_thread::get_id());

            metrics(const metrics& other) = delete;
            metrics& operator=(const metrics& other) = delete;
            metrics(metrics&& other) = delete;
            metrics& operator=(metrics&& other) = delete;
        private:
            std::mutex mtx;
            node_registry nodes;
            metrics() = default;
            ~metrics() = default;
    };
}
#endif
