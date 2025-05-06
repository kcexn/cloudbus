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
#include "../io.hpp"
#include "../formats.hpp"
#include <sstream>
#include <functional>
#pragma once
#ifndef CLOUDBUS_INTERFACES
#define CLOUDBUS_INTERFACES
namespace cloudbus {
    struct cbus_service {
        using format_type = messages::xmsgstream;
    };
    struct stream_service {
        using format_type = std::stringstream;
    };
    template<class InterfaceT>
    struct stream_traits : public InterfaceT
    {
        using Base = InterfaceT;
    };
    class interface_base {
        public:
            struct weight_type {
                std::size_t max;
                std::size_t count;
                std::size_t priority;
            };
            using stream_type = ::io::streams::sockstream;
            using native_handle_type = stream_type::native_handle_type;
            using stream_ptr = std::shared_ptr<stream_type>;
            using handle_type = std::tuple<stream_ptr, native_handle_type>;
            using handles_type = std::vector<handle_type>;
            using clock_type = std::chrono::system_clock;
            using time_point = clock_type::time_point;
            using duration_type = std::chrono::seconds;
            using ttl_type = std::tuple<time_point, duration_type>;
            using address_type = std::tuple<struct sockaddr_storage, socklen_t, ttl_type, weight_type>;
            using addresses_type = std::vector<address_type>;
            using callback_type = std::function<void(handle_type& hnd, const struct sockaddr*, socklen_t, const std::string&)>;
            using option_type = std::tuple<std::string, std::string>;
            using options_type = std::vector<option_type>;

            static const address_type NULLADDR;
            static address_type make_address(const struct sockaddr *addr, socklen_t addrlen, const ttl_type& ttl, const weight_type& weight={1,0,SIZE_MAX});
            static address_type make_address(const struct sockaddr *addr, socklen_t addrlen, ttl_type&& ttl, weight_type&& weight);
            static handle_type make_handle();
            static handle_type make_handle(int domain, int type, int protocol, std::ios_base::openmode which);
            static handle_type make_handle(native_handle_type sockfd, bool connected=false);

            explicit interface_base(const std::string& uri=std::string(), const std::string& protocol=std::string()):
                interface_base(addresses_type(), protocol, uri){}
            explicit interface_base(
                const struct sockaddr *addr,
                socklen_t addrlen,
                const std::string& protocol,
                const std::string& uri=std::string(),
                const duration_type& ttl=duration_type(-1)
            );
            explicit interface_base(
                const addresses_type& addresses,
                const std::string& protocol=std::string(),
                const std::string& uri=std::string()
            );
            explicit interface_base(
                addresses_type&& addresses,
                const std::string& protocol=std::string(),
                const std::string& uri=std::string()
            );
            explicit interface_base(interface_base&& other) noexcept;
            interface_base& operator=(interface_base&& other) noexcept;

            std::string& uri() { return _uri; }
            const std::string& uri() const { return _uri; }
            std::string scheme();
            std::string nid();
            std::string& protocol() { return _protocol; }
            options_type& options() { return _options; }
            std::size_t npending() const { return _pending.size(); }
            std::size_t total() const { return _total_weight; }
            std::string host();
            std::string port();

            const addresses_type& addresses() const { return _addresses; }
            const addresses_type& addresses(const addresses_type& addrs);
            const addresses_type& addresses(addresses_type&& addrs);

            const handles_type& streams() const { return _streams; }
            handle_type& make(handle_type&& handle=make_handle());
            handle_type& make(int domain, int type, int protocol, std::ios_base::openmode which=(std::ios_base::in | std::ios_base::out));
            handle_type& make(native_handle_type sockfd, bool connected=false);

            handles_type::iterator erase(handles_type::const_iterator it);
            handles_type::iterator erase(const handle_type& handle);

            void register_connect(const stream_ptr& ptr, callback_type&& connect_callback);

            virtual ~interface_base() = default;

            interface_base(const interface_base& other) = delete;
            interface_base& operator=(const interface_base& other) = delete;
        private:
            using callbacks_type = std::vector<std::tuple<std::weak_ptr<stream_type>, callback_type> >;

            const address_type& next();
            void _resolve_callbacks();
            std::size_t expire_addresses(const time_point& t=clock_type::now());

            std::string _uri, _protocol;
            addresses_type _addresses;
            handles_type _streams;
            callbacks_type _pending;
            std::size_t _idx, _total_weight, _prio;
            options_type _options;

            friend void swap(interface_base& lhs, interface_base& rhs) noexcept;
    };
    template<class InterfaceT, class Traits = stream_traits<InterfaceT> >
    struct interface : public interface_base
    {
        using Base = interface_base;
        using traits_type = Traits;
        using format_type = typename traits_type::format_type;
        virtual ~interface() = default;
    };
    struct cs_interface : public interface<cbus_service>
    {
        virtual ~cs_interface() = default;
    };
    struct ss_interface : public interface<stream_service>
    {
        virtual ~ss_interface() = default;
    };
}
#endif
