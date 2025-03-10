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
#include <chrono>
#include <tuple>
#include <vector>
#include <ares.h>
#pragma once
#ifndef CLOUDBUS_DNS
#define CLOUDBUS_DNS
namespace cloudbus {
    namespace dns {
        class dns_base {
            public:
                using clock_type = std::chrono::steady_clock;
                using duration_type = std::chrono::milliseconds;
                using ares_handle_type = std::tuple<ares_channel_t*, clock_type::time_point>;
                using ares_handles_type = std::vector<ares_handle_type>;
                using options_type = struct ares_options;
                static ares_handle_type make_handle(const options_type& options, const int& optmask);

                dns_base(): dns_base(options_type{}, 0){}
                explicit dns_base(const options_type& options, const int& optmask);

                ares_handles_type& handles() { return _handles; }
                const ares_handles_type& handles() const { return _handles; }
                const options_type& options() const { return _opts; }
                const int& optmask() const { return _optmask; }

                virtual ~dns_base();

                dns_base(const dns_base& other) = delete;
                dns_base(dns_base&& other) = delete;
                dns_base& operator=(const dns_base& other) = delete;
                dns_base& operator=(dns_base&& other) = delete;

            private:
                ares_handles_type _handles;
                options_type _opts;
                int _optmask;
        };
        template<class TriggerT>
        class dns_manager : public dns_base
        {
            public:
                using Base = dns_base;
                using options_type = Base::options_type;
                using trigger_type = TriggerT;

                dns_manager(trigger_type& triggers): Base(triggers, options_type{}, 0){}
                explicit dns_manager(triggers_type& triggers, const options_type& options, const int& optmask):
                    Base(options, optmask),
                    _triggers{triggers}
                {}

                trigger_type& triggers() { return _triggers; }

                virtual ~dns_manager() = default;
                dns_manager(const dns_manager& other) = delete;
                dns_manager(dns_manager&& other) = delete;
                dns_manager& operator=(const dns_manager& other) = delete;
                dns_manager& operator=(dns_manager&& other) = delete;

            private:
                trigger_type& _triggers;
        };
    }
}
#endif