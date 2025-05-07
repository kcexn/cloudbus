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
    static constexpr std::size_t span = 3;
    static constexpr stream_metrics::duration_type init_intercompletion{60*1000}, init_interarrival{0};
    static constexpr stream_metrics::clock_type::time_point init_time{init_interarrival};    
    namespace {
        template<class T>
        static bool owner_equal(std::weak_ptr<T> p1, std::weak_ptr<T> p2) {
            return !p1.owner_before(p2) && !p2.owner_before(p1);
        }
        static auto update_ewma(const stream_metrics::duration_type& delta) {
            using duration_type = stream_metrics::duration_type;
            auto denom = duration_type(span+1);
            if(delta > duration_type::max()/2)
            {
                auto overflow = delta - duration_type::max() % delta;
                auto rem1 = duration_type::max() % denom, rem2 = overflow % denom;
                return duration_type(duration_type::max()/denom +
                    overflow/denom +
                    (rem1+rem2)/denom);
            }
            else if (delta < duration_type::min()/2)
            {
                auto underflow = delta - duration_type::min() % delta;
                auto rem1 = duration_type::min() % denom, rem2 = underflow % denom;
                return duration_type(duration_type::min()/denom +
                    underflow/denom +
                    (rem1+rem2)/denom);
            } else return duration_type(2*delta/denom);
        }
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
                    *it = std::move(*--end);
                    measurements.pop_back();
                    end = measurements.end();
                } else if(owner_equal(wp, ptr)) {
                    return it;
                } else ++it;
            }
            return end;
        }
    }
    stream_metrics::duration_type stream_metrics::add_completion(
        weak_ptr ptr,
        const time_point& t
    ){
        if(ptr.expired())
            return duration_type{-1};
        std::lock_guard<std::mutex> lk(mtx);
        auto metric_it = find_metric(measurements, ptr);
        if(metric_it == measurements.end()) {
            measurements.push_back({std::move(ptr), init_interarrival, init_intercompletion, t, t});
            measurements.shrink_to_fit();
            return init_intercompletion;
        }
        auto intercompletion = std::chrono::duration_cast<duration_type>(
            t - metric_it->last_completion
        );
        metric_it->last_completion = t;
        auto delta = intercompletion - metric_it->intercompletion;
        return metric_it->intercompletion += update_ewma(delta);
    }
    stream_metrics::duration_type stream_metrics::add_arrival(
        weak_ptr ptr,
        const time_point& t
    ){
        if(ptr.expired())
            return duration_type{-1};
        std::lock_guard<std::mutex> lk(mtx);
        auto metric_it = find_metric(measurements, ptr);
        if(metric_it == measurements.end()) {
            measurements.push_back({std::move(ptr), init_interarrival, init_intercompletion, t, t});
            measurements.shrink_to_fit();
            return init_interarrival;
        }
        auto interarrival = std::chrono::duration_cast<duration_type>(
            t - metric_it->last_arrival
        );
        metric_it->last_arrival = t;
        auto delta = interarrival - metric_it->interarrival;
        return metric_it->interarrival += update_ewma(delta);
    }
    stream_metrics::metrics_vec stream_metrics::get_all_measurements() {
        std::lock_guard<std::mutex> lk(mtx);
        return measurements;
    }
    node_metrics& metrics::make_node(const std::thread::id& tid) {
        std::lock_guard<std::mutex> lk(mtx);
        auto[it, emplaced] = nodes.try_emplace(tid);
        return it->second;
    }
    std::atomic<std::size_t>& metrics::arrivals(const std::thread::id& tid) {
        std::lock_guard<std::mutex> lk(mtx);
        return nodes[tid].arrivals;
    }
    stream_metrics& metrics::streams(const std::thread::id& tid) {
        std::lock_guard<std::mutex> lk(mtx);
        return nodes[tid].streams;
    }
    metrics::metrics_vec metrics::get_all_measurements() {
        metrics_vec m;
        std::lock_guard<std::mutex> lk(mtx);
        m.reserve(nodes.size());
        for(auto&[tid, nm]: nodes) {
            m.push_back({
                tid,
                nm.arrivals.load(std::memory_order_relaxed),
                nm.streams.get_all_measurements()
            });
        }
        return m;
    }
    void metrics::erase_node(const std::thread::id& tid) {
        std::lock_guard<std::mutex> lk(mtx);
        nodes.erase(tid);
    }
}
