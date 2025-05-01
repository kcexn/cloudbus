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
    static constexpr stream_metrics::time_point init_time{init_interarrival};
    static stream_metrics::metrics_type::iterator find_metric(
        stream_metrics::metrics_type& measurements,
        stream_metrics::weak_ptr& ptr
    ){
        if(ptr.expired())
            return measurements.end();
        auto it = measurements.begin();
        while(it != measurements.end()) {
            auto& wp = it->wp;
            if(wp.expired()) {
                it = measurements.erase(it);
            } else if( !(wp.owner_before(ptr) || ptr.owner_before(wp)) ) {
                return it;
            } else ++it;
        }
        return measurements.end();
    }
    static stream_metrics::duration_type add_completion_lockfree(
        stream_metrics::metric_type& m,
        const stream_metrics::time_point& t
    ){
        using duration_type = stream_metrics::duration_type;
        auto intercompletion = std::chrono::duration_cast<duration_type>(
            t - m.last_completion
        );
        auto delta = intercompletion - m.intercompletion;
        m.last_completion = t;
        if(delta > duration_type::max()/2)
        {
            auto overflow = delta - duration_type::max() % delta;
            auto rem1 = duration_type::max() % (span+1), rem2 = overflow % (span+1);
            return m.intercompletion += duration_type::max()/(span+1) +
                overflow/(span+1) +
                (rem1+rem2)/(span+1);
        }
        else if (delta < duration_type::min()/2)
        {
            auto underflow = delta - duration_type::min() % delta;
            auto rem1 = duration_type::min() % (span+1), rem2 = underflow % (span+1);
            return m.intercompletion += duration_type::min()/(span+1) +
                underflow/(span+1) +
                (rem1+rem2)/(span+1);
        }
        else return m.intercompletion += 2*delta/(span+1);
    }
    stream_metrics::duration_type stream_metrics::add_completion(
        weak_ptr ptr
    ){
        if(ptr.expired())
            return duration_type(-1);
        metric_type *mptr = nullptr;
        time_point t;
        {
            std::lock_guard<std::mutex> lk(mtx);
            t = clock_type::now();
            auto it = find_metric(measurements, ptr);
            if(it == measurements.end()) {
                auto intercompletion = std::chrono::duration_cast<duration_type>(
                    t.time_since_epoch()
                );
                measurements.push_back({
                    ptr,
                    init_interarrival,
                    intercompletion,
                    init_time,
                    t
                });
                return intercompletion;
            } else {
                mptr = &*it;
            }
        }
        return add_completion_lockfree(*mptr, t);
    }
    static stream_metrics::duration_type add_arrival_lockfree(
        stream_metrics::metric_type& m,
        const stream_metrics::time_point& t
    ){
        using duration_type = stream_metrics::duration_type;
        auto interarrival = std::chrono::duration_cast<duration_type>(t - m.last_arrival);
        m.last_arrival = t;
        auto delta = interarrival - m.interarrival;
        if(delta > duration_type::max()/2)
        {
            auto overflow = delta - duration_type::max() % delta;
            auto rem1 = duration_type::max() % (span+1), rem2 = overflow % (span+1);
            return m.interarrival += duration_type::max()/(span+1) +
                overflow/(span+1) +
                (rem1+rem2)/(span+1);
        }
        else if (delta < duration_type::min()/2)
        {
            auto underflow = delta - duration_type::min() % delta;
            auto rem1 = duration_type::min() % (span+1), rem2 = underflow % (span+1);
            return m.interarrival += duration_type::min()/(span+1) +
                underflow/(span+1) +
                (rem1+rem2)/(span+1);
        }
        else return m.interarrival += 2*delta/(span+1);
    }
    stream_metrics::duration_type stream_metrics::add_arrival(weak_ptr ptr) {
        if(ptr.expired())
            return duration_type(-1);
        stream_metrics::metric_type *mptr = nullptr;
        stream_metrics::time_point t;
        {
            std::lock_guard<std::mutex> lk(mtx);
            t = clock_type::now();
            auto it = find_metric(measurements, ptr);
            if(it == measurements.end()) {
                measurements.push_back({
                    ptr,
                    init_interarrival,
                    init_intercompletion,
                    t, t
                });
                return init_interarrival;
            } else {
                mptr = &*it;
            }
        }
        return add_arrival_lockfree(*mptr, t);
    }
    stream_metrics::metric_type stream_metrics::find(weak_ptr ptr) {
        if(ptr.expired())
            return metric_type();
        std::lock_guard<std::mutex> lk(mtx);
        auto it = find_metric(measurements, ptr);
        return it != measurements.end() ? *it : metric_type();
    }
    stream_metrics::metrics_vec stream_metrics::get_all_measurements() {
        metrics_vec metrics;
        metrics.reserve(measurements.size());
        std::lock_guard<std::mutex> lk(mtx);
        auto it = measurements.begin();
        while(it != measurements.end()) {
            if(it->wp.expired()) {
                it = measurements.erase(it);
            } else {
                metrics.push_back(*it++);
            }
        }
        return metrics;
    }
}