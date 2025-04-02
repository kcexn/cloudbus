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
#include "interfaces.hpp"
#include <cstring>
namespace cloudbus {
    static constexpr std::size_t SHRINK_THRESHOLD = 4096;
    const interface_base::address_type interface_base::NULLADDR = interface_base::address_type{};
    interface_base::address_type interface_base::make_address(const struct sockaddr *addr, socklen_t addrlen, const ttl_type& ttl){
        auto address = address_type();
        auto& [ifaddr, len, ttl_] = address;
        len = addrlen;
        std::memset(&ifaddr, 0, sizeof(addr));
        std::memcpy(&ifaddr, addr, addrlen);
        ttl_ = ttl;
        return address;
    }
    interface_base::handle_type interface_base::make_handle(){
        return std::make_tuple(stream_type::BAD_SOCKET, std::make_shared<stream_type>());
    }
    interface_base::handle_type interface_base::make_handle(int domain, int type, int protocol, std::ios_base::openmode which){
        auto s = std::make_shared<stream_type>(domain, type, protocol, which);
        return std::make_tuple(s->native_handle(), s);
    }
    interface_base::handle_type interface_base::make_handle(int sockfd, bool connected){
        return std::make_tuple(sockfd, std::make_shared<stream_type>(sockfd, connected));
    }

    interface_base::interface_base(
        const struct sockaddr *addr,
        socklen_t addrlen,
        const std::string& protocol,
        const std::string& uri,
        const duration_type& ttl
    ):
        _uri{uri}, _protocol{protocol},
        _addresses{}, _streams{}, _pending{},
        _idx{0}
    {
        if(addr != nullptr && addrlen >= sizeof(sa_family_t))
            _addresses.push_back(make_address(addr, addrlen, std::make_tuple(clock_type::now(), ttl)));
        else throw std::invalid_argument(
            "interface_base::interface_base(const std::string& uri, "
            "const struct sockaddr *addr, socklen_t addrlen, const "
            "std::string& protocol, const duration_type& "
            "ttl=duration_type(-1)): Invalid address: addr==nullptr "
            "or addrlen < sizeof(sa_family_t)."
        );
    }
    interface_base::interface_base(
        const addresses_type& addresses,
        const std::string& protocol,
        const std::string& uri
    ):
        _uri{uri}, _protocol{protocol},
        _addresses{addresses}, _streams{}, _pending{},
        _idx{0}
    {}
    interface_base::interface_base(
        addresses_type&& addresses,
        const std::string& protocol,
        const std::string& uri
    ):
        _uri{uri}, _protocol{protocol},
        _addresses{std::move(addresses)},
        _streams{}, _pending{},
        _idx{0}
    {}
    interface_base::interface_base(interface_base&& other) noexcept:
        interface_base()
    {
        swap(*this, other);
    }
    interface_base& interface_base::operator=(interface_base&& other) noexcept {
        swap(*this, other);
        return *this;
    }
    const interface_base::addresses_type& interface_base::addresses(const addresses_type& addrs){
        _addresses = addrs;
        _addresses.shrink_to_fit();
        _resolve_callbacks();
        return _addresses;
    }
    const interface_base::addresses_type& interface_base::addresses(addresses_type&& addrs){
        _addresses = std::move(addrs);
        _addresses.shrink_to_fit();
        _resolve_callbacks();
        return _addresses;
    }
    interface_base::handle_type& interface_base::make(){
        _streams.emplace_back(make_handle());
        if(_streams.capacity() > SHRINK_THRESHOLD)
            _streams.shrink_to_fit();
        return _streams.back();
    }
    interface_base::handle_type& interface_base::make(int domain, int type, int protocol, std::ios_base::openmode which){
        _streams.emplace_back(make_handle(domain, type, protocol, which));
        if(_streams.capacity() > SHRINK_THRESHOLD)
            _streams.shrink_to_fit();
        return _streams.back();
    }
    interface_base::handle_type& interface_base::make(native_handle_type sockfd, bool connected){
        _streams.emplace_back(make_handle(sockfd, connected));
        if(_streams.capacity() > SHRINK_THRESHOLD)
            _streams.shrink_to_fit();
        return _streams.back();
    }
    interface_base::handles_type::const_iterator interface_base::erase(handles_type::const_iterator cit){
        return _streams.erase(cit);
    }
    interface_base::handles_type::iterator interface_base::erase(handles_type::iterator it){
        return _streams.erase(it);
    }
    interface_base::handles_type::const_iterator interface_base::erase(const handle_type& handle){
        auto cit = std::find_if(
                _streams.cbegin(),
                _streams.cend(),
            [&fd=std::get<native_handle_type>(handle)]
            (const auto& hnd){
                return std::get<native_handle_type>(hnd)==fd;
            }
        );
        return erase(cit);
    }
    interface_base::handles_type::iterator interface_base::erase(handle_type& handle){
        auto it = std::find_if(
                _streams.begin(),
                _streams.end(),
            [&fd=std::get<native_handle_type>(handle)]
            (auto& hnd){
                return std::get<native_handle_type>(hnd)==fd;
            }
        );
        return erase(it);
    }
    void interface_base::register_connect(const stream_ptr& ptr, const callback_type& connect_callback){
        _pending.emplace_back(ptr, connect_callback);
        if(_pending.capacity() > SHRINK_THRESHOLD)
            _pending.shrink_to_fit();
        return _resolve_callbacks();
    }
    void interface_base::register_connect(const stream_ptr& ptr, callback_type&& connect_callback){
        _pending.emplace_back(ptr, std::move(connect_callback));
        if(_pending.capacity() > SHRINK_THRESHOLD)
            _pending.shrink_to_fit();
        return _resolve_callbacks();
    }
    const interface_base::address_type& interface_base::next(){
        if(_addresses.empty())
            return NULLADDR;
        _idx = (_idx+1) % _addresses.size();
        return _addresses[_idx];
    }
    void interface_base::_resolve_callbacks(){
        constexpr duration_type HYST_WND{300};
        if(expire_addresses(clock_type::now()-HYST_WND)){
            for(auto&[wp, cb]: _pending){
                if(!wp.expired()){
                    const auto&[addr, addrlen, ttl] = next();
                    std::find_if(
                            _streams.begin(),
                            _streams.end(),
                        [&, addr=reinterpret_cast<const struct sockaddr*>(&addr)]
                        (auto& hnd){
                            auto& ptr = std::get<stream_ptr>(hnd);
                            if(!wp.owner_before(ptr))
                                cb(hnd, addr, addrlen, _protocol);
                            return !wp.owner_before(ptr);
                        }
                    );
                }
            }
            _pending.clear();
            expire_addresses();
        }
    }
    std::size_t interface_base::expire_addresses(const time_point& t){
        for(auto it=_addresses.begin(); it < _addresses.end(); ++it){
            const auto&[time, interval] = std::get<ttl_type>(*it);
            if(interval.count() > -1 && t > time+interval)
                it = --_addresses.erase(it);
        }
        return _addresses.size();
    }

    void swap(interface_base& lhs, interface_base& rhs) noexcept {
        using std::swap;
        swap(lhs._uri, rhs._uri);
        swap(lhs._addresses, rhs._addresses);
        swap(lhs._idx, rhs._idx);
        swap(lhs._streams, rhs._streams);
    }
}