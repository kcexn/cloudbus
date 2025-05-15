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
#pragma once
#ifndef CLOUDBUS_TIMERQUEUE
#define CLOUDBUS_TIMERQUEUE
namespace cloudbus {
    class TimerQueue {
        public:
            using TimePoint = std::chrono::steady_clock::time_point;
            using Callback = std::function<void()>;
            using weak_ptr = std::weak_ptr<interface_base::stream_type>;

            // Struct to hold the event details
            struct TimeoutEvent {
                Callback callback;
                weak_ptr streamPtr;
                TimePoint expiryTime;
            };

            TimerQueue() = default;
            void addEvent(weak_ptr wp, const TimePoint& time, Callback&& func);
            std::size_t processEvents();
        private:
            std::vector<TimeoutEvent> events_;
    };
}
#endif
