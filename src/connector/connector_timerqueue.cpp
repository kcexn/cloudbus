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
#include "connector_timerqueue.hpp"
namespace cloudbus {
    namespace {
        template<class T>
        static bool owner_equal(const std::weak_ptr<T>& p1, const std::weak_ptr<T>& p2){
            return !p1.owner_before(p2) && !p2.owner_before(p1);
        }
        template<class T>
        static bool shrink_to_fit(std::vector<T>& vec){
            static constexpr std::size_t THRESH = 256;
            bool resized = false;
            const std::size_t capacity = vec.capacity();
            if( capacity > THRESH &&
                vec.size() < capacity/8
            ){
                vec  = std::vector<T>(
                    std::make_move_iterator(vec.begin()),
                    std::make_move_iterator(vec.end())
                );
                resized = true;
            }
            return resized;
        }        
    }    
    void TimerQueue::addEvent(weak_ptr wp, const TimePoint& time, Callback&& func)
    {
        using std::swap;
        auto begin = events_.begin(), end = events_.end();
        auto match = end;
        for(auto it=begin; it != end; ++it) {
            if(owner_equal(wp, it->streamPtr))
                match = it;
            if(time < it->expiryTime) {
                if(match != end) {
                    swap(*match, *it);
                    return (void)(it->expiryTime = time);
                } else {
                    events_.insert(it, {std::move(func), wp, time});
                    return (void)shrink_to_fit(events_);
                }
            }
            if(match != end) {
                swap(*match, *it);
                match = it;
            }
        }
        if(match == end) {
            events_.insert(end, {std::move(func), wp, time});
            return (void)shrink_to_fit(events_);
        } else {
            match->expiryTime = time;
        }
    }
    std::size_t TimerQueue::processEvents() {
        auto currentTime = std::chrono::steady_clock::now();
        std::size_t processedCount = 0;
        auto begin = events_.begin(), end = events_.end();
        auto first_not_expired = std::find_if(
                begin,
                end,
            [&](const auto& event){
                return event.expiryTime > currentTime;
            }
        );
        if (first_not_expired == begin)
            return 0;
        std::vector<Callback> callbacks_to_run;
        callbacks_to_run.reserve(std::distance(begin, first_not_expired));
        for (auto it = begin; it != first_not_expired; ++it) {
            callbacks_to_run.push_back(std::move(it->callback));
            ++processedCount;
        }
        events_.erase(begin, first_not_expired);
        for (auto& cb : callbacks_to_run) {
            if(cb) {
                cb();
            }
        }
        return processedCount;
    }    
}
