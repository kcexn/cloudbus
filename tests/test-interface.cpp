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
#include "tests.hpp"
#include "../src/interfaces.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <cstring>
static int test_construct() {
    using namespace cloudbus;
    interface_base base{"test.localhost:8080", "TCP"};
    FAIL_IF(base.host() != "test.localhost");
    FAIL_IF(base.port() != "8080");
    FAIL_IF(base.protocol() != "TCP");

    base = interface_base{"URN:test.localhost"};
    FAIL_IF(!base.host().empty());
    FAIL_IF(!base.port().empty());
    FAIL_IF(!base.protocol().empty());
    FAIL_IF(base.scheme() != "urn");

    return TEST_PASS;
}
static int test_addresses() {
    using namespace cloudbus;
    using clock_type = interface_base::clock_type;
    using duration_type = interface_base::duration_type;
    using addresses = interface_base::addresses_type;

    interface_base base{"test.localhost:8080", "TCP"};
    addresses addrs;
    auto&[addr, addrlen, ttl, weight] = addrs.emplace_back();
    auto *in_addr = reinterpret_cast<struct sockaddr_in*>(&addr);
    in_addr->sin_family = AF_INET;
    in_addr->sin_port = htons(8080);
    inet_pton(in_addr->sin_family, "127.0.0.1", &in_addr->sin_addr);
    addrlen = sizeof(struct sockaddr_in);
    ttl = std::make_tuple(clock_type::now(), duration_type(-1));
    weight = {0,0,SIZE_MAX};
    {
        base.addresses(addrs);
        for(const auto&[addr_, addrlen_, ttl_, weight_]: addrs){
            auto *in_addr_ = reinterpret_cast<struct sockaddr_in*>(&addr);
            FAIL_IF(in_addr_->sin_family != in_addr->sin_family);
            FAIL_IF(in_addr_->sin_port != in_addr->sin_port);
            FAIL_IF(std::memcmp(&in_addr_->sin_addr, &in_addr->sin_addr, sizeof(in_addr->sin_addr)));
            FAIL_IF(addrlen_ != addrlen);
            FAIL_IF(std::memcmp(&ttl_, &ttl, sizeof(ttl)));
            FAIL_IF(std::memcmp(&weight_, &weight, sizeof(weight)));
        }
        FAIL_IF(base.total() != 1);
    }
    {
        auto addresses = base.addresses();
        base.addresses(std::move(addresses));
        for(const auto&[addr_, addrlen_, ttl_, weight_]: addrs){
            auto *in_addr_ = reinterpret_cast<struct sockaddr_in*>(&addr);
            FAIL_IF(in_addr_->sin_family != in_addr->sin_family);
            FAIL_IF(in_addr_->sin_port != in_addr->sin_port);
            FAIL_IF(std::memcmp(&in_addr_->sin_addr, &in_addr->sin_addr, sizeof(in_addr->sin_addr)));
            FAIL_IF(addrlen_ != addrlen);
            FAIL_IF(std::memcmp(&ttl_, &ttl, sizeof(ttl)));
            FAIL_IF(std::memcmp(&weight_, &weight, sizeof(weight)));
        }
        FAIL_IF(base.total() != 1);
    }
    return TEST_PASS;
}
static int test_streams() {
    using namespace cloudbus;
    using clock_type = interface_base::clock_type;
    using duration_type = interface_base::duration_type;
    using addresses = interface_base::addresses_type;
    using weight_type = interface_base::weight_type;

    interface_base base{"test.localhost:8080", "TCP"};
    {
        auto&[ptr, fd] = base.make();
        FAIL_IF(fd != ptr->BAD_SOCKET);
        FAIL_IF(ptr->native_handle() != fd);
    }
    {
        auto&[ptr, fd] = base.make(AF_INET, SOCK_STREAM, 0);
        FAIL_IF(fd == ptr->BAD_SOCKET);
        FAIL_IF(ptr->native_handle() != fd);
    }
    auto& streams_ = base.streams();
    FAIL_IF(streams_.size() != 2);
    {
        auto&[_, fd] = streams_.back();
        auto&[ptr, fd_] = base.make(fd);
        FAIL_IF(fd_ != fd);
        FAIL_IF(fd_ == ptr->BAD_SOCKET);
        FAIL_IF(ptr->native_handle() != fd_);
    }
    FAIL_IF(streams_.size() != 3);
    base.erase(streams_.back());
    FAIL_IF(streams_.size() != 2);
    const auto& hnd = streams_.back();
    base.erase(hnd);
    FAIL_IF(streams_.size() != 1);
    auto&[ptr, fd] = streams_.back();
    base.register_connect(
        ptr,
        [&](
            auto& handle,
            const auto *sockaddr,
            auto socklen,
            const auto& protocol
        ){
            return;
        }
    );
    FAIL_IF(base.npending() != 1);
    addresses addrs;
    auto&[addr, addrlen, ttl, weight] = addrs.emplace_back();
    auto *in_addr = reinterpret_cast<struct sockaddr_in*>(&addr);
    in_addr->sin_family = AF_INET;
    in_addr->sin_port = htons(8080);
    inet_pton(in_addr->sin_family, "127.0.0.1", &in_addr->sin_addr);
    addrlen = sizeof(struct sockaddr_in);
    ttl = std::make_tuple(clock_type::now(), duration_type(0));
    weight = {1,0,0};
    base.addresses(addrs);
    FAIL_IF(base.npending() != 0);
    FAIL_IF(!base.addresses().empty());

    base.register_connect(
        ptr,
        [&](
            auto& handle,
            const auto *sockaddr,
            auto socklen,
            const auto& protocol
        ){
            return;
        }
    );
    FAIL_IF(base.npending() != 1);
    ttl = std::make_tuple(clock_type::now(), duration_type(300));
    weight = {1,0,SIZE_MAX};
    base.addresses(addrs);
    auto addrs_ = base.addresses();
    FAIL_IF(addrs_.empty());
    for(auto& addr: addrs_)
        FAIL_IF(std::get<weight_type>(addr).count != 1);
    return TEST_PASS;
}
int main(int argc, char **argv) {
    std::cout << "=============================== TEST INTERFACES ================================" << std::endl;
    EXEC_TEST(test_construct);
    EXEC_TEST(test_addresses);
    EXEC_TEST(test_streams);
    std::cout << "================================================================================" << std::endl;
    return TEST_PASS;
}