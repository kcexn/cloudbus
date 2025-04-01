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
#include "../io.hpp"
#include "../interfaces.hpp"
#include <ares.h>
#pragma once
#ifndef CLOUDBUS_DNS
#define CLOUDBUS_DNS
namespace cloudbus {
    namespace dns {
        class resolver_base
        {
            public:
                using socket_handle = std::tuple<ares_socket_t, std::uint16_t>;
                using socket_handles = std::vector<socket_handle>;
                using clock_type = std::chrono::steady_clock;
                using time_point = clock_type::time_point;
                using duration_type = std::chrono::milliseconds;
                using timeout_type = std::tuple<time_point, duration_type>;
                enum socket_state : std::uint16_t {
                    READABLE = 1 << 0,
                    WRITABLE = 1 << 2
                };
                resolver_base();
                socket_handles& handles() { return _handles; }
                const timeout_type& timeout() const { return _timeout; }
                duration_type resolve(interface_base& interface);
                duration_type process_event(const socket_handle& hnd);
                ~resolver_base();

                resolver_base(const resolver_base& other) = delete;
                resolver_base& operator=(const resolver_base& other) = delete;
                resolver_base(resolver_base&& other) = delete;
                resolver_base& operator=(resolver_base&& other) = delete;

            private:
                duration_type set_timeout();

                socket_handles _handles;
                timeout_type _timeout;
                ares_channel _channel;
                ares_options _opts;
        };

        template<class HandlerT>
        class basic_resolver : public HandlerT, public resolver_base
        {
            public:
                using Base = resolver_base;
                using trigger_type = typename HandlerT::trigger_type;
                using event_mask = typename HandlerT::event_mask;
                using events_type = typename HandlerT::events_type;
                using size_type = typename HandlerT::size_type;
                basic_resolver(trigger_type& triggers):
                    HandlerT(triggers), Base(){}
                virtual ~basic_resolver() = default;

                basic_resolver() = delete;
                basic_resolver(const basic_resolver& other) = delete;
                basic_resolver& operator=(const basic_resolver& other) = delete;
                basic_resolver(basic_resolver&& other) = delete;
                basic_resolver& operator=(basic_resolver&& other) = delete;
        };
    }
}
#endif