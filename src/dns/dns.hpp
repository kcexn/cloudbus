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
                using clock_type = std::chrono::steady_clock;
                using time_point = clock_type::time_point;
                using duration_type = std::chrono::milliseconds;
                using timeout_type = std::tuple<time_point, duration_type>;
                enum socket_state : std::uint16_t {
                    READABLE = 1 << 0,
                    WRITABLE = 1 << 2
                };
                resolver_base();
                const socket_handle& channel_handle() const { return _handle; }
                const timeout_type& timeout() const { return _timeout; }
                duration_type resolve(interface_base& interface, const char *hostname, const char *port, const struct ares_addrinfo_hints *hints);
                duration_type process_event();
                ~resolver_base();

                resolver_base(const resolver_base& other) = delete;
                resolver_base& operator=(const resolver_base& other) = delete;
                resolver_base(resolver_base&& other) = delete;
                resolver_base& operator=(resolver_base&& other) = delete;

            private:
                duration_type set_timeout();

                socket_handle _handle;
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

            protected:
                virtual size_type _handle(events_type& events) override {
                    size_type handled = 0;
                    if(auto& sockfd=std::get<ares_socket_t>(channel_handle());
                        sockfd != ARES_SOCKET_BAD
                    ){
                        for(auto& event: events)
                            if(event.revents && event.fd==sockfd && ++handled)
                                process_event();
                        if(events.empty() && process_event().count() > -1)
                            ++handled;
                    }
                    return handled;
                }

        };
    }
}
#endif