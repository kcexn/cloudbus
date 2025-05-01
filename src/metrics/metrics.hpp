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
#include <atomic>
#include <list>
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
            using time_point = clock_type::time_point;

            using weak_ptr = std::weak_ptr<stream_type>;
            struct metric_type {
                weak_ptr wp;
                duration_type interarrival, intercompletion;
                time_point last_arrival, last_completion;
            };
            using metrics_type = std::list<metric_type>;
            using metrics_vec = std::vector<metric_type>;
            static inline stream_metrics& get() {
                static stream_metrics m;
                return m;
            }

            duration_type add_completion(weak_ptr ptr);
            duration_type add_arrival(weak_ptr ptr);
            metric_type find(weak_ptr ptr);
            metrics_vec get_all_measurements();

            stream_metrics(const stream_metrics& other) = delete;
            stream_metrics& operator=(const stream_metrics& other) = delete;
            stream_metrics(stream_metrics&& other) = delete;
            stream_metrics& operator=(stream_metrics&& other) = delete;
        private:
            std::mutex mtx;
            metrics_type measurements;
            stream_metrics() = default;
            ~stream_metrics() = default;
    };
    class metrics {
        public:
            using counter_type = std::atomic<std::size_t>;
            static inline metrics& get() {
                static metrics m;
                return m;
            }

            /* carried traffic = offered traffic unless offered traffic *
             * exceeds capacity. I only need to count arrivals if I am  *
             * interested in throughput. I will need to compute capacity*
             * using a different method.                                */
            counter_type arrivals;
            static inline auto& get_stream_metrics() {
                return stream_metrics::get();
            }

            metrics(const metrics& other) = delete;
            metrics& operator=(const metrics& other) = delete;
            metrics(metrics&& other) = delete;
            metrics& operator=(metrics&& other) = delete;

        private:
            metrics() = default;
            ~metrics() = default;
    };
}
#endif