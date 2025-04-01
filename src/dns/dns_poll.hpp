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
#include "dns.hpp"
#pragma once
#ifndef CLOUDBUS_DNS_POLL
#define CLOUDBUS_DNS_POLL
namespace cloudbus {
    namespace dns {
        template<>
        class basic_resolver<handler_type> : public handler_type, public resolver_base
        {
            public:
                using Base = resolver_base;
                basic_resolver(trigger_type& triggers):
                    handler_type(triggers), Base(){}
                virtual ~basic_resolver() = default;

                basic_resolver() = delete;
                basic_resolver(const basic_resolver& other) = delete;
                basic_resolver& operator=(const basic_resolver& other) = delete;
                basic_resolver(basic_resolver&& other) = delete;
                basic_resolver& operator=(basic_resolver&& other) = delete;

            protected:
                virtual size_type _handle(events_type& events) override {
                    size_type handled = 0;
                    for(auto hit=handles().begin(); hit < handles().end(); ++hit){
                        auto& hnd = *hit;
                        auto&[sockfd, sockev] = hnd;
                        event_mask set=0, unset=0;
                        auto cit = std::find_if(
                                events.cbegin(),
                                events.cend(),
                            [&](const auto& event){
                                if(event.fd==sockfd){
                                    set = event.events;
                                    if(event.revents && ++handled)
                                        process_event(hnd);
                                }
                                return event.fd==sockfd;
                            }
                        );
                        if(cit == events.cend()){
                            auto&[time, interval] = timeout();
                            if(clock_type::now() > time+interval && ++handled)
                                process_event(hnd);
                        } else cit=events.erase(cit);
                        if(!sockev){
                            this->triggers().clear(sockfd);
                            hit = --handles().erase(hit);
                        } else {
                            if( (sockev & READABLE) && !(set & POLLIN) )
                                set |= POLLIN;
                            if( !(sockev & READABLE) && (set & POLLIN) )
                                unset |= POLLIN;
                            if( (sockev & WRITABLE) &&  !(set & POLLOUT) )
                                set |= POLLOUT;
                            if( !(sockev & WRITABLE) && (set & POLLOUT) )
                                unset |= POLLOUT;
                            if(set)
                                this->triggers().set(sockfd, set);
                            if(unset)
                                this->triggers().clear(sockfd, unset);
                        }
                    }
                    return handled;
                }
        };
    }
}
#endif