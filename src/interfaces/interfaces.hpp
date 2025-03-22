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
            using stream_type = ::io::streams::sockstream;
            using native_handle_type = stream_type::native_handle_type;
            using stream_ptr = std::shared_ptr<stream_type>;
            using handle_type = std::tuple<native_handle_type, stream_ptr>;
            using handle_ptr = std::shared_ptr<handle_type>;
            using handles_type = std::vector<handle_ptr>;
            using address_type = std::tuple<struct sockaddr_storage, socklen_t>;
            using addresses_type = std::vector<address_type>;
            using callback_type = std::function<void(const handle_ptr&, const struct sockaddr*, socklen_t, const std::string&)>;
            using clock_type = std::chrono::system_clock;
            using time_point = clock_type::time_point;
            using duration_type = std::chrono::seconds;
            using dns_ttl = std::tuple<time_point, duration_type>;
         
            static const address_type NULLADDR;
            static address_type make_address(const struct sockaddr *addr, socklen_t addrlen);
            static handle_ptr make_handle();
            static handle_ptr make_handle(int domain, int type, int protocol);
            static handle_ptr make_handle(native_handle_type sockfd, bool connected=false);

            interface_base(const std::string& protocol=std::string(), const std::string& url=std::string()):
                interface_base(addresses_type(), protocol, url){}
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
                const std::string& uri=std::string(),
                const duration_type& ttl=duration_type(-1)
            );
            explicit interface_base(
                addresses_type&& addresses,
                const std::string& protocol=std::string(),
                const std::string& uri=std::string(),
                const duration_type& ttl=duration_type(-1)
            );

            std::string& uri() { return _uri; }
            std::string& protocol() { return _protocol; }

            const address_type& address();
            const addresses_type& addresses() const { return _addresses; }
            const addresses_type& addresses(const addresses_type& addrs, const duration_type& ttl=duration_type(-1));
            const addresses_type& addresses(addresses_type&& addrs, const duration_type& ttl=duration_type(-1));

            const handles_type& streams() const { return _streams; }
            handle_ptr& make();
            handle_ptr& make(int domain, int type, int protocol);
            handle_ptr& make(native_handle_type sockfd);
            handle_ptr& make(native_handle_type sockfd, bool connected);

            handles_type::const_iterator erase(handles_type::const_iterator it);
            handles_type::iterator erase(handles_type::iterator it);
            handles_type::const_iterator erase(const handle_ptr& handle);
            handles_type::iterator erase(handle_ptr& handle);

            void register_connect(std::weak_ptr<handle_type> wp, const callback_type& connect_callback);
            void register_connect(std::weak_ptr<handle_type> wp, callback_type&& connect_callback);

            virtual ~interface_base() = default;

            interface_base(const interface_base& other) = delete;
            interface_base& operator=(const interface_base& other) = delete;
            interface_base(interface_base&& other) = delete;
            interface_base& operator=(interface_base&& other) = delete;
        private:
            using callbacks_type = std::vector<std::tuple<std::weak_ptr<handle_type>, callback_type> >;

            std::string _uri, _protocol;
            addresses_type _addresses;
            std::size_t _idx;
            handles_type _streams;
            callbacks_type _pending;
            dns_ttl _timeout;

            void _resolve_callbacks();
            void _expire_addresses();
    };

    template<class InterfaceT, class Traits = stream_traits<InterfaceT> >
    class interface : public interface_base
    {
        public:
            using Base = interface_base;
            using traits_type = Traits;
            using format_type = typename traits_type::format_type;

            interface(const std::string& protocol=std::string(), const std::string& url=std::string()):
                interface(addresses_type(), protocol, url){}
            explicit interface(
                const struct sockaddr *addr,
                socklen_t addrlen,
                const std::string& protocol,
                const std::string& uri=std::string(),
                const duration_type& ttl=duration_type(-1)
            ): Base(addr, addrlen, protocol, uri, ttl){}
            explicit interface(
                const addresses_type& addresses,
                const std::string& protocol=std::string(),
                const std::string& uri=std::string(),
                const duration_type& ttl=duration_type(-1)
            ): Base(addresses, protocol, uri, ttl){}

            virtual ~interface() = default;

            interface(const interface& other) = delete;
            interface& operator=(const interface& other) = delete;
    };

    class cs_interface: public interface<cbus_service>
    {
        public:
            using Base = interface<cbus_service>;

            cs_interface(const std::string& protocol=std::string(), const std::string& url=std::string()):
                cs_interface(addresses_type(), protocol, url){}
            explicit cs_interface(
                const struct sockaddr *addr,
                socklen_t addrlen,
                const std::string& protocol,
                const std::string& uri=std::string(),
                const duration_type& ttl=duration_type(-1)
            ): Base(addr, addrlen, protocol, uri, ttl){}
            explicit cs_interface(
                const addresses_type& addresses,
                const std::string& protocol=std::string(),
                const std::string& uri=std::string(),
                const duration_type& ttl=duration_type(-1)
            ): Base(addresses, protocol, uri, ttl){}
            explicit cs_interface(
                addresses_type&& addresses,
                const std::string& protocol=std::string(),
                const std::string& uri=std::string(),
                const duration_type& ttl=duration_type(-1)
            ): Base(std::move(addresses), protocol, uri, ttl){}

            virtual ~cs_interface() = default;

            cs_interface(const cs_interface& other) = delete;
            cs_interface& operator=(const cs_interface& other) = delete;
            cs_interface(cs_interface&& other) = delete;
            cs_interface& operator=(cs_interface&& other) = delete;
    };

    class ss_interface: public interface<stream_service>
    {
        public:
            using Base = interface<stream_service>;

            ss_interface(const std::string& protocol=std::string(), const std::string& url=std::string()):
                ss_interface(addresses_type(), protocol, url){}
            explicit ss_interface(
                const struct sockaddr *addr,
                socklen_t addrlen,
                const std::string& protocol,
                const std::string& uri=std::string(),
                const duration_type& ttl=duration_type(-1)
            ): Base(addr, addrlen, protocol, uri, ttl){}
            explicit ss_interface(
                const addresses_type& addresses,
                const std::string& protocol=std::string(),
                const std::string& uri=std::string(),
                const duration_type& ttl=duration_type(-1)
            ): Base(addresses, protocol, uri, ttl){}
            explicit ss_interface(
                addresses_type&& addresses,
                const std::string& protocol=std::string(),
                const std::string& uri=std::string(),
                const duration_type& ttl=duration_type(-1)
            ): Base(std::move(addresses), protocol, uri, ttl){}

            virtual ~ss_interface() = default;

            ss_interface(const ss_interface& other) = delete;
            ss_interface& operator=(const ss_interface& other) = delete;
            ss_interface(ss_interface&& other) = delete;
            ss_interface& operator=(ss_interface&& other) = delete;
    };  
}
#endif
