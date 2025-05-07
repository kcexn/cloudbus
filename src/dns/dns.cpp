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
#include "dns.hpp"
#include <pcre2.h>
#include <charconv>
#include <mutex>
#include <arpa/nameser.h>
#include <cstring>
namespace cloudbus {
    namespace dns {
        using addrinfo_args = std::tuple<interface_base&, struct ares_addrinfo_hints*, interface_base::weight_type>;
        using ares_query_args = std::tuple<interface_base&, const ares_channel&, std::string, std::string>;
        static void resolve_ares_getaddrinfo (
            interface_base& iface,
            const ares_channel& channel,
            const char *name,
            const char *service,
            const interface_base::weight_type& weight={1,0,SIZE_MAX}
        );
        static void resolve_ares_getsrv(
            interface_base& iface,
            const ares_channel& channel,
            const std::string& name
        );
        static void resolve_ares_getnaptr(
            interface_base& iface,
            const ares_channel& channel,
            const std::string& name,
            const std::string& subject
        );        
        static void ares_addrinfo_success(
            interface_base& iface,
            const interface_base::weight_type& weight,
            struct ares_addrinfo *result
        ){
            using addresses_type = interface_base::addresses_type;
            using clock_type = interface_base::clock_type;
            using duration_type = interface_base::duration_type;

            addresses_type addresses;
            for(auto *node = result->nodes; node; node = node->ai_next){
                auto&[addr, addrlen, ttl, weight_] = addresses.emplace_back();
                std::memcpy(&addr, node->ai_addr, node->ai_addrlen);
                addrlen = node->ai_addrlen;
                ttl = std::make_tuple(clock_type::now(), duration_type(node->ai_ttl));
                weight_ = weight;
            }
            iface.addresses(std::move(addresses));
            ares_freeaddrinfo(result);
        }
        static const char *ares_handle_error(interface_base& iface, int status){
            using addresses_type = interface_base::addresses_type;
            using clock_type = interface_base::clock_type;
            using duration_type = interface_base::duration_type;

            addresses_type addrs;
            auto&[addr, addrlen, ttl, weight] = addrs.emplace_back();
            std::memset(&addr, 0, sizeof(addr));
            addrlen = static_cast<socklen_t>(-1);
            ttl = std::make_tuple(clock_type::now(), duration_type(0));
            weight = {1,0,SIZE_MAX};
            iface.addresses(std::move(addrs));
            return ares_strerror(status);
        }
        static std::string extract_protocol(const std::string& name) {
            auto start = std::find(name.cbegin(), name.cend(), '.'),
                end = std::find(++start, name.cend(), '.');
            while(!std::isalpha(*(++start)));
            auto proto = std::string(start, end);
            std::transform(
                    proto.begin(), proto.end(),
                    proto.begin(),
                [](unsigned char c){
                    return std::toupper(c);
                }
            );
            return proto;
        }
        static void ares_parse_srv_success(
            interface_base& iface,
            const ares_channel& channel,
            const std::string& name,
            struct ares_srv_reply *srv
        ){
            using weight_type = interface_base::weight_type;
            constexpr std::size_t PORT_CHARBUF_SIZE = 6;

            std::array<char, PORT_CHARBUF_SIZE> port = {};
            iface.protocol() = extract_protocol(name);
            char *begin=port.data(), *end=port.data()+port.max_size();
            for(auto *cur=srv; cur != nullptr; cur=cur->next) {
                auto[ptr, ec] = std::to_chars(begin, end, cur->port);
                if(ec != std::errc())
                    throw std::system_error(
                        std::make_error_code(ec),
                        "Unable to convert port number to char*."
                    );
                *ptr = '\0';
                resolve_ares_getaddrinfo(
                    iface, channel,
                    cur->host, port.data(),
                    weight_type{cur->weight, 0, cur->priority}
                );
            }
            ares_free_data(srv);
        }
        static const unsigned char *ares_match_naptr_flag(struct ares_naptr_reply *cur) {
            for(auto *flag=cur->flags; flag && *flag != '\0'; ++flag) {
                switch(*flag=std::tolower(*flag)) {
                    case 's':
                        return flag;
                    default:
                        return nullptr;
                }
            }
            return reinterpret_cast<const unsigned char*>("");
        }
        static int naptr_resolve_sub(
            interface_base& iface,
            const ares_channel& channel,
            const std::string& subject,
            const char *substitution,
            const unsigned char *flag
        ){
            switch(*flag){
                case '\0':
                    resolve_ares_getnaptr(
                        iface, channel,
                        substitution,
                        subject
                    );
                    return 0;
                case 's':
                    resolve_ares_getsrv(
                        iface, channel,
                        substitution
                    );
                    return 0;
                default:
                    return -1;
            }
        }
        static std::string extract_part(const unsigned char *regexp, std::size_t len) {
            if(!regexp || !len)
                return std::string();
            auto delim=*regexp;
            const auto *end = ++regexp;
            while(--len != 0 && *end != '\0' && *end != delim)
                ++end;
            return std::string(reinterpret_cast<const char*>(regexp), end-regexp);
        }
        static void replace_backreferences(std::string& s) {
            for(auto it=s.begin(); it != s.end(); ++it){
                if(*it == '\\') {
                    if(++it == s.end())
                        return;
                    if( std::isdigit(*it) && *it != '0' ) {
                        *(it-1) = '$';
                    }
                }
            }
        }
        static void tolower(std::string& match, std::string& substitute, std::string& subject){
            std::transform(
                    match.begin(), match.end(),
                    match.begin(),
                [](unsigned char c){ return std::tolower(c); }
            );
            std::transform(
                    substitute.begin(), substitute.end(),
                    substitute.begin(),
                [](unsigned char c){ return std::tolower(c); }
            );
            std::transform(
                    subject.begin(), subject.end(),
                    subject.begin(),
                [](unsigned char c){ return std::tolower(c); }
            );
        }
        static int pcre_substitute_(
            struct ares_naptr_reply *cur,
            std::size_t regexp_size,
            const std::string& subject,
            PCRE2_UCHAR **result,
            PCRE2_SIZE *result_len
        ){
            pcre2_code *re = nullptr;
            pcre2_match_data *match_data = nullptr;
            auto subj = subject;
            int rc = 0;
            PCRE2_SIZE roff = 0;
            auto strsize = std::strlen(reinterpret_cast<const char*>(cur->regexp));
            auto match = extract_part(cur->regexp, strsize);
            auto substitute = extract_part(cur->regexp+match.size()+1, strsize-match.size()-1);
            replace_backreferences(substitute);
            if( *(cur->regexp+strsize-1) == 'i' )
                tolower(match, substitute, subj);
            re = pcre2_compile(
                reinterpret_cast<PCRE2_SPTR8>(match.c_str()),
                match.size(),
                PCRE2_NEVER_UCP | PCRE2_NEVER_UTF,
                &rc,
                &roff,
                nullptr
            );
            if(re) {
                match_data = pcre2_match_data_create_from_pattern(re, nullptr);
                do {
                    if(auto *ptr = std::realloc(*result, (*result_len)*sizeof(PCRE2_UCHAR)))
                        *result = static_cast<PCRE2_UCHAR*>(ptr);
                    else throw std::bad_alloc();
                    rc = pcre2_substitute(
                        re,
                        reinterpret_cast<PCRE2_SPTR>(subj.c_str()),
                        PCRE2_ZERO_TERMINATED,
                        0,
                        PCRE2_SUBSTITUTE_OVERFLOW_LENGTH,
                        match_data,
                        nullptr,
                        reinterpret_cast<PCRE2_SPTR>(substitute.c_str()),
                        PCRE2_ZERO_TERMINATED,
                        *result,
                        result_len
                    );
                } while(rc == PCRE2_ERROR_NOMEMORY);
            }
            pcre2_match_data_free(match_data);
            pcre2_code_free(re);
            return rc;
        }
        static int ares_parse_naptr_success(
            interface_base& iface,
            const ares_channel& channel,
            const std::string& subject,
            struct ares_naptr_reply *naptr
        ){
            PCRE2_UCHAR *result = nullptr;
            auto *cur=naptr;
            for(; cur; cur=cur->next) {
                const unsigned char *flag = ares_match_naptr_flag(cur);
                if(!flag)
                    continue;
                auto regexp_size = !cur->regexp ? 0 :
                    std::strlen(reinterpret_cast<const char*>(cur->regexp));
                auto replacement_size = !cur->replacement ? 0 :
                    std::strlen(cur->replacement);
                if(regexp_size && replacement_size)
                    continue;
                if(regexp_size) {
                    PCRE2_SIZE result_len = 256;
                    int rc = pcre_substitute_(
                        cur,
                        regexp_size,
                        subject,
                        &result,
                        &result_len
                    );
                    if(rc > 0 && result_len) {
                        rc = naptr_resolve_sub(
                            iface, channel,
                            subject,
                            reinterpret_cast<const char*>(result),
                            flag
                        );
                        if(!rc)
                            break;
                    }
                } else if(replacement_size) {
                    int rc = naptr_resolve_sub(
                        iface, channel,
                        subject,
                        cur->replacement,
                        flag
                    );
                    if(!rc)
                        break;
                }
            }
            int rc = cur ? 0 : -1;
            std::free(result);
            ares_free_data(naptr);
            return rc;
        }
        static void ares_getsrv_success(
            interface_base& iface,
            const ares_channel& channel,
            const std::string& name,
            unsigned char *abuf,
            int alen
        ){
            struct ares_srv_reply *srv = nullptr;
            int rc = ares_parse_srv_reply(abuf, alen, &srv);
            switch(rc) {
                case ARES_SUCCESS:
                    return ares_parse_srv_success(iface, channel, name, srv);
                default:
                    std::cerr << __FILE__ << ':' << __LINE__
                        << ":ares error:"
                        << ares_handle_error(iface, rc)
                        << std::endl;
                    break;
            }
        }
        static void ares_getnaptr_success(
            interface_base& iface,
            const ares_channel& channel,
            const std::string& subject,
            unsigned char *abuf,
            int alen
        ){
            struct ares_naptr_reply *naptr = nullptr;
            int rc = ares_parse_naptr_reply(abuf, alen, &naptr);
            switch(rc) {
                case ARES_SUCCESS:
                    if(ares_parse_naptr_success(iface, channel, subject, naptr)) {
                        std::cerr << __FILE__ << ":" << __LINE__
                            << ":ares error:" << ares_handle_error(iface, rc)
                            << ":no NAPTR match." << std::endl;
                    }
                    break;
                default:
                    std::cerr << __FILE__ << ':' << __LINE__
                        << ":ares error:"
                        << ares_handle_error(iface, rc)
                        << std::endl;
                    break;
            }
        }
        extern "C" {
            static void ares_socket_cb(void *data, ares_socket_t socket_fd, int readable, int writable) {
                using socket_handle = resolver_base::socket_handle;
                using socket_handles = resolver_base::socket_handles;
                auto *hnds = static_cast<socket_handles*>(data);
                auto lb = std::lower_bound(
                        hnds->begin(), hnds->end(), socket_handle{socket_fd, 0},
                    [](const auto& lhs, const auto& rhs) {
                        return std::get<ares_socket_t>(lhs) < std::get<ares_socket_t>(rhs);
                    }
                );
                if(lb == hnds->end() || std::get<ares_socket_t>(*lb) != socket_fd) {
                    lb = hnds->insert(lb, {socket_fd, 0});
                    if(hnds->size() < hnds->capacity()/4)
                        hnds->shrink_to_fit();
                }
                auto&[sockfd, sockstate] = *lb;
                sockfd = socket_fd;
                sockstate = 0;
                if(readable)
                    sockstate |= resolver_base::READABLE;
                if(writable)
                    sockstate |= resolver_base::WRITABLE;
            }
            static void ares_addrinfo_cb(
                void *arg,
                int status,
                int timeouts,
                struct ares_addrinfo *result
            ){
                auto *args = static_cast<addrinfo_args*>(arg);
                auto&[iface, hints, weight] = *args;
                delete hints;
                switch(status) {
                    case ARES_SUCCESS:
                        ares_addrinfo_success(iface, weight, result);
                        break;
                    default:
                        std::cerr << __FILE__ << ':' << __LINE__
                            << ":ares error="
                            << ares_handle_error(iface, status)
                            << std::endl;
                        break;
                }
                delete args;
            }
            static void ares_getsrv_cb(
                void *arg, int status,
                int timeouts,
                unsigned char *abuf,
                int alen
            ){
                auto *args = static_cast<ares_query_args*>(arg);
                auto&[iface, channel, name, subject] = *args;
                switch(status) {
                    case ARES_SUCCESS:
                        ares_getsrv_success(iface, channel, name, abuf, alen);
                        break;
                    default:
                        std::cerr << __FILE__ << ':' << __LINE__
                            << ":name=" << name
                            << " ares error="
                            << ares_handle_error(iface, status)
                            << std::endl;
                        break;
                }
                delete args;
            }
            static void ares_getnaptr_cb(
                void *arg, int status,
                int timeouts,
                unsigned char *abuf,
                int alen
            ){
                auto *args = static_cast<ares_query_args*>(arg);
                auto&[iface, channel, name, subject] = *args;
                switch(status) {
                    case ARES_SUCCESS:
                        ares_getnaptr_success(iface, channel, subject, abuf, alen);
                        break;
                    default:
                        std::cerr << __FILE__ << ':' << __LINE__
                            << ":name=" << name
                            << " subject=" << subject
                            << " ares error="
                            << ares_handle_error(iface, status)
                            << std::endl;
                        break;
                }
                delete args;
            }
        }
        static std::mutex ares_library_mtx;
        static int ares_version_number=0, ares_initialized=0;
        static void initialize_ares_library(){
            const std::lock_guard<std::mutex> lock(ares_library_mtx);
            if(ares_initialized == INT_MAX)
                throw std::runtime_error( 
                    "libc-ares has been initialized more than INT_MAX times."
                );
            if( !(ares_initialized++) ){
                const char *v = ares_version(&ares_version_number);
                if(ares_version_number < 0x011201){
                    std::string what = "c-ares version " + std::string(v)
                                    + " is unsupported. Requires "
                                    + "c-ares version >= 1.18.1.";
                    throw std::runtime_error(what);
                }
                if(int err = ares_library_init(ARES_LIB_INIT_ALL)){
                    std::string what = "c-ares failed to initialize with error: "
                                    + std::string(ares_strerror(err));
                    throw std::runtime_error(what);
                }
            }
        }
        static void cleanup_ares_library(){
            const std::lock_guard<std::mutex> lock(ares_library_mtx);
            if( !(--ares_initialized) )
                ares_library_cleanup();
        }
        static void initialize_ares_channel(
            ares_channel *channel,
            ares_options *opts,
            int mask
        ){
            int status = ares_init_options(channel, opts, mask);
            switch(status){
                case ARES_ENOTINITIALIZED:
                    throw std::runtime_error("The c-ares library is not initialized.");
                case ARES_ENOMEM:
                    throw std::bad_alloc();
                case ARES_EFILE:
                    throw std::runtime_error(
                        "c-ares channel failed to initialize "
                        "because a DNS configuration file could "
                        "not be read."
                    );
                default:
                    return;
            }
        }
        resolver_base::resolver_base():
            _handles{}, _timeout{clock_type::now(), -1},
            _channel{}, _opts{}
        {
            initialize_ares_library();
            _opts.timeout = 500;
            _opts.sock_state_cb = ares_socket_cb;
            _opts.sock_state_cb_data = &_handles;
            initialize_ares_channel(&_channel, &_opts, ARES_OPT_SOCK_STATE_CB | ARES_OPT_TIMEOUTMS | ARES_OPT_ROTATE);
        }
        static void resolve_ares_getaddrinfo(
            interface_base& iface,
            const ares_channel& channel,
            const char *name,
            const char *service,
            const interface_base::weight_type& weight
        ){
            struct ares_addrinfo_hints *hints = new struct ares_addrinfo_hints;
            std::memset(hints, 0, sizeof(struct ares_addrinfo_hints));
            hints->ai_family = AF_INET;
            if(iface.protocol() == "TCP")
                hints->ai_socktype = SOCK_STREAM;
            addrinfo_args *args = new addrinfo_args{iface, hints, weight};
            return ares_getaddrinfo(
                channel,
                name, service,
                hints,
                ares_addrinfo_cb,
                args
            );
        }
        static void resolve_ares_getsrv(
            interface_base& iface,
            const ares_channel& channel,
            const std::string& name
        ){
            auto *args = new ares_query_args{iface, channel, name, ""};
            return ares_query(
                channel,
                name.c_str(),
                ns_c_in, ns_t_srv,
                ares_getsrv_cb,
                args
            );
        }
        static void resolve_ares_getnaptr(
            interface_base& iface,
            const ares_channel& channel,
            const std::string& name,
            const std::string& subject
        ){
            auto *args = new ares_query_args{iface, channel, name, subject};
            return ares_query(
                channel,
                name.c_str(),
                ns_c_in, ns_t_naptr,
                ares_getnaptr_cb,
                args
            );
        }
        resolver_base::duration_type resolver_base::resolve(interface_base& iface) {
            if(iface.protocol().empty()) {
                auto& uri = iface.uri();
                const auto scheme = iface.scheme();
                if(scheme=="srv") {
                    resolve_ares_getsrv(iface, _channel, uri.substr(scheme.size()+1));
                } else if(scheme=="naptr") {
                    resolve_ares_getnaptr(iface, _channel, uri.substr(scheme.size()+1), uri.substr(scheme.size()+1));
                } else if(scheme=="urn") {
                    const auto nid = iface.nid();
                    resolve_ares_getnaptr(iface, _channel, nid+"."+scheme, uri);
                } else {
                    resolve_ares_getnaptr(iface, _channel, scheme+".uri", uri);
                }
            } else if(iface.protocol() == "TCP") {
                resolve_ares_getaddrinfo(
                    iface, _channel,
                    iface.host().c_str(),
                    iface.port().c_str()
                );
            } else throw std::invalid_argument(
                "resolver_base::resolve(interface_base& iface) - Invalid protocol, "+iface.protocol()
            );
            return set_timeout();
        }
        resolver_base::duration_type resolver_base::process_event(const socket_handle& hnd){
            ares_socket_t readfd=ARES_SOCKET_BAD, writefd=ARES_SOCKET_BAD;
            const auto&[sockfd, sockev] = hnd;
            const auto&[time, interval] = _timeout;
            if(clock_type::now() < time+interval){
                if(sockev & READABLE)
                    readfd = sockfd;
                if(sockev & WRITABLE)
                    writefd = sockfd;
            }
            ares_process_fd(_channel, readfd, writefd);
            return set_timeout();
        }
        resolver_base::~resolver_base(){
            ares_destroy(_channel);
            cleanup_ares_library();
        }
        resolver_base::duration_type resolver_base::set_timeout(){
            struct timeval tv={};
            if( ares_timeout(_channel, nullptr, &tv) ){
                auto interval = duration_type(tv.tv_sec*1000+tv.tv_usec/1000);
                _timeout = timeout_type{clock_type::now(), interval};
                return interval;
            }
            _timeout = timeout_type{clock_type::now(), -1};
            return duration_type(-1);
        }
    }
}