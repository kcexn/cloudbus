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
#include "interfaces.hpp"
#include <cstring>
namespace cloudbus {
    namespace {
        template<class T>
        static bool owner_equal(const std::shared_ptr<T>& p1, const std::shared_ptr<T>& p2){
            return !p1.owner_before(p2) && !p2.owner_before(p1);
        }
    }    
    const interface_base::address_type interface_base::NULLADDR = interface_base::address_type{};
    interface_base::address_type interface_base::make_address(const struct sockaddr *addr, socklen_t addrlen, const ttl_type& ttl, const weight_type& weight){
        auto address = address_type();
        auto&[ifaddr, len, ttl_, weight_] = address;
        len = addrlen;
        std::memset(&ifaddr, 0, sizeof(addr));
        std::memcpy(&ifaddr, addr, addrlen);
        ttl_ = ttl;
        weight_ = weight;
        return address;
    }
    interface_base::address_type interface_base::make_address(const struct sockaddr *addr, socklen_t addrlen, ttl_type&& ttl, weight_type&& weight){
        auto address = address_type();
        auto&[ifaddr, len, ttl_, weight_] = address;
        len = addrlen;
        std::memset(&ifaddr, 0, sizeof(addr));
        std::memcpy(&ifaddr, addr, addrlen);
        ttl_ = std::move(ttl);
        weight_ = weight;
        return address;
    }
    interface_base::handle_type interface_base::make_handle(){
        return std::make_tuple(std::make_shared<stream_type>(), stream_type::BAD_SOCKET);
    }
    interface_base::handle_type interface_base::make_handle(int domain, int type, int protocol, std::ios_base::openmode which){
        auto s = std::make_shared<stream_type>(domain, type, protocol, which);
        auto fd = s->native_handle();
        return std::make_tuple(std::move(s), fd);
    }
    interface_base::handle_type interface_base::make_handle(int sockfd, bool connected){
        return std::make_tuple(std::make_shared<stream_type>(sockfd, connected), sockfd);
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
        _idx{0}, _total_weight{0}, _prio{SIZE_MAX},
        _options{}
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
        _idx{0}, _total_weight{0}, _prio{SIZE_MAX},
        _options{}
    {}
    interface_base::interface_base(
        addresses_type&& addresses,
        const std::string& protocol,
        const std::string& uri
    ):
        _uri{uri}, _protocol{protocol},
        _addresses{std::move(addresses)},
        _streams{}, _pending{},
        _idx{0}, _total_weight{0}, _prio{SIZE_MAX},
        _options{}
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
    std::string interface_base::scheme() {
        auto delim = std::find(_uri.begin(), _uri.end(), ':');
        auto s = std::string(_uri.begin(), delim);
        std::transform(
                s.begin(), s.end(),
                s.begin(),
            [](unsigned char c) {
                return std::tolower(c);
            }
        );
        return s;
    }
    std::string interface_base::nid() {
        if(scheme() == "urn") {
            auto begin = _uri.begin()+4,
                end = std::find(begin, _uri.end(), ':');
            auto s = std::string(begin, end);
            std::transform(
                    s.begin(), s.end(),
                    s.begin(),
                [](unsigned char c) {
                    return std::tolower(c);
                }
            );
            return s;
        }
        return std::string();
    }
    static std::string_view port_(const std::string& uri) {
        auto delim = std::find(uri.begin(), uri.end(), ':');
        if(delim == uri.end() || ++delim == uri.end())
            return std::string_view();
        auto start = delim;
        while(std::isdigit(*delim) && ++delim != uri.end());
        return delim==uri.end() ?
            std::string_view(&*start, delim-start) :
            std::string_view();
    }
    std::string interface_base::port() {
        return std::string(port_(_uri));
    }
    std::string interface_base::host() {
        auto p = port_(_uri);
        if(p.empty())
            return std::string();
        auto host = std::string(_uri.data(), p.data()-_uri.data()-1);
        std::transform(
                host.begin(), host.end(),
                host.begin(),
            [](unsigned char c){
                return std::tolower(c);
            }
        );
        return host;
    }
    static auto find_weights(interface_base::addresses_type& addresses){
        using addresses_type = interface_base::addresses_type;
        using iterator = addresses_type::iterator;
        using weight_type = interface_base::weight_type;
        using iterators = std::vector<iterator>;

        std::size_t weight=0, prio=SIZE_MAX;
        iterators its;
        for(auto it=addresses.begin(); it != addresses.end(); ++it) {
            auto& prio_ = std::get<weight_type>(*it).priority;
            if(prio_ < prio) {
                prio = prio_;
                weight = 0;
                its.clear();
            }
            if(prio_ == prio) {
                its.push_back(it);
                weight += std::get<weight_type>(*it).max;
            }
        }
        if(!weight) {
            for(auto& it: its)
                std::get<weight_type>(*it).max = 1;
            weight = its.size();
        }
        return std::make_tuple(weight, prio);
    }
    const interface_base::addresses_type& interface_base::addresses(const addresses_type& addrs){
        _addresses=addrs;
        auto[weight, prio] = find_weights(_addresses);
        _total_weight=weight, _prio=prio;
        _resolve_callbacks();
        return _addresses;
    }
    const interface_base::addresses_type& interface_base::addresses(addresses_type&& addrs){
        _addresses = std::move(addrs);
        auto[weight, prio] = find_weights(_addresses);
        _total_weight=weight, _prio=prio;
        _resolve_callbacks();
        return _addresses;
    }
    interface_base::handle_type& interface_base::make(handle_type&& hnd) {
        auto ub = std::upper_bound(
                _streams.begin(), _streams.end(), hnd,
            [&](const auto& lhs, const auto& rhs) {
                auto& lptr = std::get<stream_ptr>(lhs);
                auto& rptr = std::get<stream_ptr>(rhs);
                return lptr.owner_before(rptr);
            }
        );
        ub = _streams.insert(ub, std::move(hnd));
        return *ub;
    }
    interface_base::handle_type& interface_base::make(int domain, int type, int protocol, std::ios_base::openmode which){
        return make(make_handle(domain, type, protocol, which));
    }
    interface_base::handle_type& interface_base::make(native_handle_type sockfd, bool connected){
        return make(make_handle(sockfd, connected));
    }
    interface_base::handles_type::iterator interface_base::erase(handles_type::const_iterator cit){
        return _streams.erase(cit);
    }
    interface_base::handles_type::iterator interface_base::erase(const handle_type& handle) {
        auto cit = std::lower_bound(
                _streams.cbegin(),
                _streams.cend(),
                handle,
            [&](const auto& lhs, const auto& rhs) {
                const auto& lptr = std::get<stream_ptr>(lhs);
                const auto& rptr = std::get<stream_ptr>(rhs);
                return lptr.owner_before(rptr);
            }
        );
        if(cit != _streams.cend()) {
            const auto& hptr = std::get<stream_ptr>(handle);
            const auto& ptr = std::get<stream_ptr>(*cit);
            if(owner_equal(hptr, ptr))
                return erase(cit);
        }
        return _streams.end();
    }
    void interface_base::register_connect(const stream_ptr& ptr, callback_type&& connect_callback){
        _pending.emplace_back(ptr, std::move(connect_callback));
        return _resolve_callbacks();
    }
    static void clear_counters(interface_base::addresses_type& addresses){
        using weight_type = interface_base::weight_type;
        for(auto& addr: addresses)
            std::get<weight_type>(addr).count = 0;
    }
    const interface_base::address_type& interface_base::next(){
        if(_addresses.empty())
            return NULLADDR;
        std::size_t sum = 0;
        while(sum < _total_weight) {
            _idx = (_idx+1) % _addresses.size();
            auto& addr = _addresses[_idx];
            auto&[max, count, prio] = std::get<weight_type>(addr);
            if(prio == _prio) {
                if(count++ < max)
                    return addr;
                sum += --count;
            }
        }
        clear_counters(_addresses);
        return next();
    }
    void interface_base::_resolve_callbacks(){
        constexpr duration_type HYST_WND{300};
        if(expire_addresses(clock_type::now()-HYST_WND)){
            auto pending{std::move(_pending)};
            _pending = callbacks_type();
            for(auto&[wp, cb]: pending) {
                if(!wp.expired()) {
                    const auto&[addr, addrlen, ttl, weight] = next();
                    const auto *addrp = addrlen != static_cast<socklen_t>(-1) ?
                        reinterpret_cast<const struct sockaddr*>(&addr) :
                        nullptr;
                    for(auto& hnd: _streams) {
                        const auto& ptr = std::get<stream_ptr>(hnd);
                        if( !(ptr.owner_before(wp) || wp.owner_before(ptr)) ) {
                            cb(hnd, addrp, addrlen, _protocol);
                            break;
                        }
                    }
                }
            }
            expire_addresses();
        }
    }
    std::size_t interface_base::expire_addresses(const time_point& t){
        auto end = std::remove_if(
                _addresses.begin(), _addresses.end(),
            [&](auto& addr){
                const auto&[time, interval] = std::get<ttl_type>(addr);
                return (interval.count() > -1 && t > time+interval) ||
                    !std::get<weight_type>(addr).max;
            }
        );
        _addresses.erase(end, _addresses.end());
        auto[weight, prio] = find_weights(_addresses);
        _total_weight = weight, _prio = prio;
        if(_addresses.empty() &&
            ( scheme() == "srv" ||
            scheme() == "naptr" ||
            scheme() == "urn" )
        ){
            protocol().clear();
        }
        return _addresses.size();
    }
    void swap(interface_base& lhs, interface_base& rhs) noexcept {
        using std::swap;
        swap(lhs._uri, rhs._uri);
        swap(lhs._protocol, rhs._protocol);
        swap(lhs._addresses, rhs._addresses);
        swap(lhs._streams, rhs._streams);
        swap(lhs._pending, rhs._pending);
        swap(lhs._idx, rhs._idx);
        swap(lhs._total_weight, rhs._total_weight);
        swap(lhs._prio, rhs._prio);
        swap(lhs._options, rhs._options);
    }
}
