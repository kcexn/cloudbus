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
    static constexpr stream_metrics::duration_type init_response{60*1000}, init_interarrival{0};
    static constexpr stream_metrics::time_point init_time{init_interarrival};
    static stream_metrics::metrics_type::iterator find_metric(
        stream_metrics::metrics_type& measurements,
        stream_metrics::weak_ptr& ptr
    ){
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
    stream_metrics::duration_type stream_metrics::update_response_time(weak_ptr ptr, const duration_type& t) {
        stream_metrics::metric_type *mptr = nullptr;
        {
            std::lock_guard<std::mutex> lk(mtx);
            auto it = find_metric(measurements, ptr);
            if(it == measurements.end()) {
                measurements.push_back({ptr, t, init_interarrival, init_time});
                return t;
            } else {
                mptr = &*it;
            }
        }
        auto& m = *mptr;
        auto delta = t - m.response;
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
        else return m.response += 2*delta/(span+1);
    }
    stream_metrics::duration_type stream_metrics::add_arrival(weak_ptr ptr) {
        stream_metrics::metric_type *mptr = nullptr;
        stream_metrics::time_point t;
        {
            std::lock_guard<std::mutex> lk(mtx);
            t = clock_type::now();
            auto it = find_metric(measurements, ptr);
            if(it == measurements.end()) {
                auto interval = std::chrono::duration_cast<duration_type>(t.time_since_epoch());
                measurements.push_back({ptr, init_response, interval, t});
                return interval;
            } else {
                mptr = &*it;
            }
        }
        auto& m = *mptr;
        auto interval = std::chrono::duration_cast<duration_type>(t - m.last);
        m.last = t;
        auto delta = interval - m.interarrival;
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
    stream_metrics::metric_type stream_metrics::find(weak_ptr ptr) {
        std::lock_guard<std::mutex> lk(mtx);
        auto it = find_metric(measurements, ptr);
        return it != measurements.end() ? *it : metric_type();
    }
    stream_metrics::metrics_type stream_metrics::get_all_measurements() {
        std::lock_guard<std::mutex> lk(mtx);
        auto it = measurements.begin();
        while(it != measurements.end()) {
            if((it++)->wp.expired())
                it = measurements.erase(--it);
        }
        return measurements;
    }
}