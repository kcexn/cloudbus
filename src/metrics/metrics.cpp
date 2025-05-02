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
#include "metrics.hpp"
namespace cloudbus {
    namespace {
        template<class T>
        static bool owner_equal(std::weak_ptr<T> p1, std::weak_ptr<T> p2) {
            return !p1.owner_before(p2) && !p2.owner_before(p1);
        }
    }
    static constexpr std::size_t span = 3;
    static constexpr stream_metrics::duration_type init_intercompletion{60*1000}, init_interarrival{0};
    static constexpr stream_metrics::clock_type::time_point init_time{init_interarrival};
    static stream_metrics::metrics_vec::iterator find_metric(
        stream_metrics::metrics_vec& measurements,
        stream_metrics::weak_ptr& ptr
    ){
        if(ptr.expired())
            return measurements.end();
        auto it = measurements.begin(), end = measurements.end();
        while(it != end) {
            auto& wp = it->wp;
            if(wp.expired()) {
                *it = std::move(*(--end));
            } else if(owner_equal(wp, ptr)) {
                measurements.resize(end-measurements.begin());
                return it;
            } else ++it;
        }
        measurements.resize(end-measurements.begin());
        return measurements.end();
    }
    stream_metrics::duration_type stream_metrics::add_completion(
        weak_ptr ptr
    ){
        const auto t = clock_type::now();
        auto metric_it = find_metric(measurements, ptr);
        if(metric_it == measurements.end()) {
            measurements.push_back({ptr, init_interarrival, init_intercompletion, t, t});
            measurements.shrink_to_fit();
            return init_intercompletion;
        }
        auto intercompletion = std::chrono::duration_cast<duration_type>(
            t - metric_it->last_completion
        );
        metric_it->last_completion = t;
        auto delta = intercompletion - metric_it->intercompletion;
        if(delta > duration_type::max()/2)
        {
            auto overflow = delta - duration_type::max() % delta;
            auto rem1 = duration_type::max() % (span+1), rem2 = overflow % (span+1);
            return metric_it->intercompletion += duration_type::max()/(span+1) +
                overflow/(span+1) +
                (rem1+rem2)/(span+1);
        }
        else if (delta < duration_type::min()/2)
        {
            auto underflow = delta - duration_type::min() % delta;
            auto rem1 = duration_type::min() % (span+1), rem2 = underflow % (span+1);
            return metric_it->intercompletion += duration_type::min()/(span+1) +
                underflow/(span+1) +
                (rem1+rem2)/(span+1);
        }
        else return metric_it->intercompletion += 2*delta/(span+1);
    }
    stream_metrics::duration_type stream_metrics::add_arrival(
        weak_ptr ptr
    ){
        const auto t = clock_type::now();
        auto metric_it = find_metric(measurements, ptr);
        if(metric_it == measurements.end()) {
            measurements.push_back({ptr, init_interarrival, init_intercompletion, t, t});
            measurements.shrink_to_fit();
            return init_interarrival;
        }
        auto interarrival = std::chrono::duration_cast<duration_type>(
            t - metric_it->last_arrival
        );
        metric_it->last_arrival = t;
        auto delta = interarrival - metric_it->interarrival;
        if(delta > duration_type::max()/2)
        {
            auto overflow = delta - duration_type::max() % delta;
            auto rem1 = duration_type::max() % (span+1), rem2 = overflow % (span+1);
            return metric_it->interarrival += duration_type::max()/(span+1) +
                overflow/(span+1) +
                (rem1+rem2)/(span+1);
        }
        else if (delta < duration_type::min()/2)
        {
            auto underflow = delta - duration_type::min() % delta;
            auto rem1 = duration_type::min() % (span+1), rem2 = underflow % (span+1);
            return metric_it->interarrival += duration_type::min()/(span+1) +
                underflow/(span+1) +
                (rem1+rem2)/(span+1);
        }
        else return metric_it->interarrival += 2*delta/(span+1);
    }
    node_metrics& metrics::make_node(const std::thread::id& tid) {
        std::lock_guard<std::mutex> lk(mtx);
        auto[it, emplaced] = nodes.try_emplace(tid);
        return it->second;
    }
    std::size_t& metrics::arrivals(const std::thread::id& tid) {
        std::lock_guard<std::mutex> lk(mtx);
        return nodes[tid].arrivals;
    }
    stream_metrics& metrics::streams(const std::thread::id& tid) {
        std::lock_guard<std::mutex> lk(mtx);
        return nodes[tid].streams;
    }
    metrics::node_metrics_vec metrics::get_all_measurements() {
        std::lock_guard<std::mutex> lk(mtx);
        return node_metrics_vec(nodes.begin(), nodes.end());
    }
    void metrics::erase_node(const std::thread::id& tid) {
        std::lock_guard<std::mutex> lk(mtx);
        nodes.erase(tid);
    }
}
