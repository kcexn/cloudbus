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
    const interface_base::address_type interface_base::NULLADDR = interface_base::address_type{};
    interface_base::address_type interface_base::make_address(const struct sockaddr *addr, socklen_t addrlen){
        auto address = address_type();
        auto& [ifaddr, len] = address;
        len = addrlen;
        std::memset(&ifaddr, 0, sizeof(addr));
        std::memcpy(&ifaddr, addr, addrlen);
        return address;
    }
    interface_base::handle_ptr interface_base::make_handle(){
        return std::make_shared<handle_type>(0, std::make_shared<stream_type>());
    }
    interface_base::handle_ptr interface_base::make_handle(int domain, int type, int protocol){
        auto s = std::make_shared<stream_type>(domain, type, protocol); 
        return std::make_shared<handle_type>(s->native_handle(), s);
    }
    interface_base::handle_ptr interface_base::make_handle(int sockfd){
        return std::make_shared<handle_type>(sockfd, std::make_shared<stream_type>(sockfd));
    }
    interface_base::handle_ptr interface_base::make_handle(int sockfd, bool connected){
        return std::make_shared<handle_type>(sockfd, std::make_shared<stream_type>(sockfd, connected));
    }

    interface_base::interface_base(const std::string& uri, const struct sockaddr *addr, socklen_t addrlen, const std::string& protocol, const duration_type& ttl):
        _uri{uri}, _protocol{protocol},
        _addresses{}, _idx{0}, _streams{}, _pending{}, 
        _timeout{clock_type::now(), ttl}
    {
        if(addr != nullptr && addrlen >= sizeof(sa_family_t))
            _addresses.push_back(make_address(addr, addrlen));
        else throw std::invalid_argument(
            "interface_base::interface_base(const std::string& uri, "
            "const struct sockaddr *addr, socklen_t addrlen, const "
            "std::string& protocol, const duration_type& "
            "ttl=duration_type(-1)): Invalid address: addr==nullptr "
            "or addrlen < sizeof(sa_family_t)."
        );
    }
    interface_base::interface_base(const std::string& uri, const addresses_type& addresses, const std::string& protocol, const duration_type& ttl):
        _uri{uri}, _protocol{protocol},
        _addresses{addresses}, _idx{0}, _streams{}, _pending{},
        _timeout{clock_type::now(), ttl}
    {}
    interface_base::interface_base(const std::string& uri, addresses_type&& addresses, const std::string& protocol, const duration_type& ttl):
        _uri{uri}, _protocol{protocol},
        _addresses{std::move(addresses)}, _idx{0}, _streams{}, _pending{},
        _timeout{clock_type::now(), ttl}
    {}

    interface_base::interface_base(interface_base&& other):
        _uri{std::move(other._uri)}, _protocol(std::move(other._protocol)),
        _addresses{std::move(other._addresses)}, _idx{other._idx}, 
        _streams{std::move(other._streams)}, _pending{std::move(other._pending)},
        _timeout{std::move(other._timeout)}
    { other._idx = 0; }
    interface_base& interface_base::operator=(interface_base&& other){
        _uri = std::move(other._uri);
        _protocol = std::move(other._protocol);
        _addresses = std::move(other._addresses);
        _idx = other._idx; other._idx = 0;
        _streams = std::move(other._streams);
        _pending = std::move(other._pending);
        _timeout = std::move(other._timeout);
        return *this;
    }
    const interface_base::address_type& interface_base::address(){
        if(_addresses.empty())
            return NULLADDR;
        _idx = (_idx + 1) % _addresses.size();
        return _addresses[_idx];
    }
    const interface_base::addresses_type& interface_base::addresses(const addresses_type& addrs, const duration_type& ttl){
        _timeout = std::make_tuple(clock_type::now(), ttl);
        _addresses = addrs;
        _resolve_callbacks();
        return _addresses;
    }
    const interface_base::addresses_type& interface_base::addresses(addresses_type&& addrs, const duration_type& ttl){
        _timeout = std::make_tuple(clock_type::now(), ttl);
        _addresses = std::move(addrs);
        _resolve_callbacks();
        return _addresses;
    }
    interface_base::handle_ptr& interface_base::make(){
        _streams.push_back(make_handle());
        return _streams.back();
    }
    interface_base::handle_ptr& interface_base::make(int domain, int type, int protocol){
        _streams.push_back(make_handle(domain, type, protocol));
        return _streams.back();
    }
    interface_base::handle_ptr& interface_base::make(native_handle_type sockfd){
        _streams.push_back(make_handle(sockfd));
        return _streams.back();
    }
    interface_base::handle_ptr& interface_base::make(native_handle_type sockfd, bool connected){
        _streams.push_back(make_handle(sockfd, connected));
        return _streams.back();
    }
    interface_base::handles_type::const_iterator interface_base::erase(handles_type::const_iterator cit){ 
        return _streams.erase(cit); 
    }
    interface_base::handles_type::iterator interface_base::erase(handles_type::iterator it){ 
        return _streams.erase(it); 
    }
    interface_base::handles_type::const_iterator interface_base::erase(const handle_ptr& handle){
        auto cit = std::find(_streams.cbegin(), _streams.cend(), handle);
        return erase(cit);
    }
    interface_base::handles_type::iterator interface_base::erase(handle_ptr& handle){
        auto it = std::find(_streams.begin(), _streams.end(), handle);
        return erase(it);
    }
    void interface_base::register_connect(std::weak_ptr<handle_type> wp, const callback_type& connect_callback){
        _pending.emplace_back(wp, connect_callback);
        return _resolve_callbacks();
    }
    void interface_base::register_connect(std::weak_ptr<handle_type> wp, callback_type&& connect_callback){
        _pending.emplace_back(wp, std::move(connect_callback));
        return _resolve_callbacks();
    }
    void interface_base::_resolve_callbacks(){
        if(!_addresses.empty()){
            for(auto&[wp, cb]: _pending){
                if(auto ptr = wp.lock()){
                    const auto&[addr, addrlen] = address();
                    cb(ptr, reinterpret_cast<const struct sockaddr*>(&addr), addrlen);
                }
            }
            _pending.clear();
            if(auto&[time, ttl] = _timeout;
                ttl.count() >= 0 && clock_type::now() >= time+ttl)
                return _expire_addresses();
        }
    }
    void interface_base::_expire_addresses(){
        _addresses.clear();
        _timeout = std::make_tuple(clock_type::now(), duration_type(-1));
    }
}