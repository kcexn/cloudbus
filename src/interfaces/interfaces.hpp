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

#pragma once
#ifndef CLOUDBUS_INTERFACES
#define CLOUDBUS_INTERFACES
namespace cloudbus {
    struct cbus_service{};
    struct stream_service{};
    struct service_registration{};

    template<class InterfaceT> struct stream_traits;
    template<>
    struct stream_traits<cbus_service>{
        using stream_type = ::io::streams::sockstream;
        using native_handle_type = stream_type::native_handle_type;
        using stream_ptr = std::shared_ptr<stream_type>;
        using handle_type = std::tuple<native_handle_type, stream_ptr>;
        using storage_type = struct sockaddr_storage;
        using size_type = socklen_t;
        using address_type = struct sockaddr*;
        using format_type = messages::xmsgstream;
        static handle_type make_stream(int domain, int type, int protocol){ 
            auto s = std::make_shared<stream_type>(domain, type, protocol); 
            return std::make_tuple(s->native_handle(),s);
        }
        static handle_type make_stream(native_handle_type sockfd) { 
            return std::make_tuple(sockfd, std::make_shared<stream_type>(sockfd));
        }
    };
    template<>
    struct stream_traits<stream_service>{
        using stream_type = ::io::streams::sockstream;
        using native_handle_type = stream_type::native_handle_type;
        using stream_ptr = std::shared_ptr<stream_type>;
        using handle_type = std::tuple<native_handle_type, stream_ptr>;     
        using storage_type = struct sockaddr_storage;
        using size_type = socklen_t;
        using address_type = struct sockaddr*;
        using format_type = std::stringstream;
        static handle_type make_stream(int domain, int type, int protocol){ 
            auto s = std::make_shared<stream_type>(domain, type, protocol); 
            return std::make_tuple(s->native_handle(),s);
        }
        static handle_type make_stream(native_handle_type sockfd) { 
            return std::make_tuple(sockfd, std::make_shared<stream_type>(sockfd));
        }
    };
    template<>
    struct stream_traits<service_registration>{
        using stream_type = ::io::streams::sockstream;
        using native_handle_type = stream_type::native_handle_type;
        using stream_ptr = std::shared_ptr<stream_type>;
        using handle_type = std::tuple<native_handle_type, stream_ptr>;     
        using storage_type = struct sockaddr_storage;
        using size_type = socklen_t;
        using address_type = struct sockaddr*;
        using format_type = messages::xmsgstream;
        static handle_type make_stream(int domain, int type, int protocol){ 
            auto s = std::make_shared<stream_type>(domain, type, protocol); 
            return std::make_tuple(s->native_handle(),s);
        }
        static handle_type make_stream(native_handle_type sockfd) { 
            return std::make_tuple(sockfd, std::make_shared<stream_type>(sockfd));
        }
    };
    template<class InterfaceT, class Traits = stream_traits<InterfaceT> >
    class interface {
        public:
            using traits_type = Traits;
            using stream_ptr = typename traits_type::stream_ptr;
            using native_handle_type = typename traits_type::native_handle_type;
            using stream_type = typename traits_type::handle_type;
            using streams_type = std::vector<stream_type>;
            using storage_type = typename traits_type::storage_type;
            using size_type = typename traits_type::size_type;
            using address_type = typename traits_type::address_type;

            interface() = default;
            interface(const address_type addr, size_type addrlen)
            :   _addrlen{addrlen}
            { std::memcpy(&_address, addr, addrlen); }
            interface(interface&& other)
                : _streams{std::move(other._streams)},
                    _addrlen{std::move(other._addrlen)}
            { 
                std::memcpy(&_address, &other._address, other._addrlen);
                other._address = {};
                other._addrlen = 0;
            }

            interface& operator=(interface&& other){
                _streams = std::move(other._streams);
                std::memcpy(&_address, &other._address, other._addrlen);
                _addrlen = std::move(other._addrlen);
                other._address = {};
                other._addrlen = 0;
                return *this;
            }

            streams_type& streams() { return _streams; }
            const address_type address() { return reinterpret_cast<const address_type>(&_address); }
            const size_type& addrlen() const { return _addrlen; }

            stream_type& make(int domain, int type, int protocol){ return _make(domain, type, protocol); }
            stream_type& make(native_handle_type sockfd){ return _make(sockfd); }

            typename streams_type::const_iterator erase(typename streams_type::const_iterator it){ return _erase(it); }
            typename streams_type::iterator erase(typename streams_type::iterator it){ return _erase(it); }
            typename streams_type::const_iterator erase(const stream_type& sp){ return _erase(sp); }
            typename streams_type::iterator erase(stream_type& sp){ return _erase(sp); }

            virtual ~interface() = default;

            interface(const interface& other) = delete;
            interface& operator=(const interface& other) = delete;

        protected:
            virtual stream_type& _make(int domain, int type, int protocol){
                streams().push_back(traits_type::make_stream(domain, type, protocol));
                return streams().back();
            }
            virtual stream_type& _make(native_handle_type sockfd){
                streams().push_back(traits_type::make_stream(sockfd));
                return streams().back();
            }
            virtual typename streams_type::const_iterator _erase(typename streams_type::const_iterator it){ return streams().erase(it); }
            virtual typename streams_type::iterator _erase(typename streams_type::iterator it){ return streams().erase(it); }
            virtual typename streams_type::const_iterator _erase(const stream_type& sp){
                auto it = std::find_if(streams().cbegin(), streams().cend(), [&](const auto& h){ return std::get<native_handle_type>(h) == std::get<native_handle_type>(sp); });
                return _erase(it);
            }
            virtual typename streams_type::iterator _erase(stream_type& sp){
                auto it = std::find_if(streams().begin(), streams().end(), [&](const auto& h){ return std::get<native_handle_type>(h) == std::get<native_handle_type>(sp); });
                return _erase(it);
            }

        private:
            streams_type _streams;
            storage_type _address;
            size_type _addrlen;
    };

    class cs_interface: public interface<cbus_service>
    {
        public:
            using Base = interface<cbus_service>;
            using traits_type = Base::traits_type;
            using stream_ptr = Base::stream_ptr;
            using native_handle_type = Base::native_handle_type;
            using stream_type = Base::stream_type;
            using streams_type = Base::stream_type;
            using storage_type = Base::storage_type;
            using size_type = Base::size_type;
            using address_type = Base::address_type;

            cs_interface() = default;
            cs_interface(const address_type addr, size_type addrlen);
            cs_interface(cs_interface&& other);

            cs_interface& operator=(cs_interface&& other);

            virtual ~cs_interface() = default;

            cs_interface(const cs_interface& other) = delete;
            cs_interface& operator=(const cs_interface& other) = delete;
    };

    class ss_interface: public interface<stream_service>
    {
        public:
            using Base = interface<stream_service>;
            using traits_type = Base::traits_type;
            using stream_ptr = Base::stream_ptr;
            using native_handle_type = Base::native_handle_type;
            using stream_type = Base::stream_type;
            using streams_type = Base::stream_type;
            using storage_type = Base::storage_type;
            using size_type = Base::size_type;
            using address_type = Base::address_type;

            ss_interface();
            ss_interface(const address_type addr, size_type addrlen);
            ss_interface(ss_interface&& other);

            ss_interface& operator=(ss_interface&& other);

            ss_interface(const ss_interface& other) = delete;
            ss_interface& operator=(const ss_interface& other) = delete;
    };  
}
#endif
