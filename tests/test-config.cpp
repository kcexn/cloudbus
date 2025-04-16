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
#include "tests.hpp"
#include "../src/config.hpp"
#include <sstream>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <cstring>
static int test_ingest_config() {
    using namespace cloudbus;
    SET_LOG_PREFIX("test_ingest_config()");
    config::configuration conf;
    std::stringstream ss;
    ss << "[Cloudbus]\n"
        << "[Test Service]\n"
        << "bind=unix:///var/run/test.sock\n"
        << "backend=unix:///var/run/backend.sock\n";
    ss >> conf;
    for(auto& section: conf.sections()){
        if(section.heading == "Cloudbus"){
            if(!section.config.empty()) {
                ERROR_LOG("section.heading==\"Cloudbus\" && !section.config.empty()");
                return TEST_FAIL;
            }
        } else if (section.heading == "Test Service"){
            if(section.config.empty()){
                ERROR_LOG("section.heading==\"Test Service\" && section.config.empty()");
                return TEST_FAIL;
            }
            for(auto&[k,v]: section.config){
                if(k == "bind") {
                    if(v != "unix:///var/run/test.sock") {
                        ERROR_LOG("section.heading==\"Test Service\" && "
                            "k==bind && v != unix:///var/run/test.sock");
                        return TEST_FAIL;
                    }
                } else if (k == "backend"){
                    if(v != "unix:///var/run/backend.sock"){
                        ERROR_LOG("section.heading==\"Test Service\" && "
                            "k == backend && v != unix:///var/run/backend.sock");
                        return TEST_FAIL;
                    }
                } else {
                    ERROR_LOG("section.heading==\"Test Service\" && "
                        "k != bind && k != backend");
                    return TEST_FAIL;
                }
            }
        } else {
            ERROR_LOG("section.heading != \"Cloudbus\" && section.heading != \"Test Service\"");
            return TEST_FAIL;
        }
    }
    return TEST_PASS;
}
static int test_make_ipv4_address() {
    using namespace cloudbus;
    SET_LOG_PREFIX("test_make_ipv4_address()");
    auto address = config::make_address("tcp://192.168.1.1:80\n");
    if(address.index() != config::SOCKADDR){
        ERROR_LOG("address.index() != config::SOCKADDR");
        return TEST_FAIL;
    }
    auto&[protocol, addr_storage, len] = std::get<config::SOCKADDR>(address);
    if(protocol != "TCP") {
        ERROR_LOG("protocol != \"TCP\"");
        return TEST_FAIL;
    }
    if(addr_storage.ss_family != AF_INET){
        ERROR_LOG("addr_storage.ss_family != AF_INET");
        return TEST_FAIL;
    }
    auto *in_addr = reinterpret_cast<struct sockaddr_in*>(&addr_storage);
    if(in_addr->sin_port != htons(80)) {
        ERROR_LOG("in_addr->sin_port != htons(80)")
        return TEST_FAIL;
    }
    char dst[INET_ADDRSTRLEN];
    std::memset(dst, 0, sizeof(dst));
    const char *net = inet_ntop(in_addr->sin_family, &in_addr->sin_addr, dst, sizeof(dst));
    if(std::strncmp(net, "192.168.1.1", INET_ADDRSTRLEN)) {
        ERROR_LOG("std::strncmp(net, \"192.168.1.1\", INET_ADDRSTRLEN)");
        return TEST_FAIL;
    }
    return TEST_PASS;
}
static int test_make_ipv6_address() {
    using namespace cloudbus;
    SET_LOG_PREFIX("test_make_ipv6_address()");
    auto address = config::make_address("tcp://[::1]:80\n");
    if(address.index() != config::SOCKADDR){
        ERROR_LOG("address.index() != config::SOCKADDR");
        return TEST_FAIL;
    }
    auto&[protocol, addr_storage, len] = std::get<config::SOCKADDR>(address);
    if(protocol != "TCP") {
        ERROR_LOG("protocol != \"TCP\"");
        return TEST_FAIL;
    }
    if(addr_storage.ss_family != AF_INET6){
        ERROR_LOG("addr_storage.ss_family != AF_INET6");
        return TEST_FAIL;
    }
    auto *in_addr = reinterpret_cast<struct sockaddr_in6*>(&addr_storage);
    if(in_addr->sin6_port != htons(80)) {
        ERROR_LOG("in_addr->sin6_port != htons(80)");
        return TEST_FAIL;
    }
    char dst[INET6_ADDRSTRLEN];
    std::memset(dst, 0, sizeof(dst));
    const char *net = inet_ntop(in_addr->sin6_family, &in_addr->sin6_addr, dst, sizeof(dst));
    if(std::strncmp(net, "::1", INET6_ADDRSTRLEN)) {
        ERROR_LOG("std::strncmp(net, \"::1\", INET6_ADDRSTRLEN)");
        return TEST_FAIL;
    }
    return TEST_PASS;
}
static int test_make_unix_address() {
    using namespace cloudbus;
    SET_LOG_PREFIX("test_make_unix_address()");
    auto address = config::make_address("unix:///var/run/test.sock\n");
    if(address.index() != config::SOCKADDR){
        ERROR_LOG("address.index() != config::SOCKADDR");
        return TEST_FAIL;
    }
    auto&[protocol, addr_storage, len] = std::get<config::SOCKADDR>(address);
    if(protocol != "UNIX") {
        ERROR_LOG("protocol != \"UNIX\"");
        return TEST_FAIL;
    }
    if(addr_storage.ss_family != AF_UNIX){
        ERROR_LOG("addr_storage.ss_family != AF_UNIX");
        return TEST_FAIL;
    }
    auto *un_addr = reinterpret_cast<struct sockaddr_un*>(&addr_storage);
    if( std::strncmp(un_addr->sun_path, "/var/run/test.sock", sizeof(un_addr->sun_path)) ) {
        ERROR_LOG("std::strncmp(un_addr->sun_path, \"/var/run/test.sock\", sizeof(un_addr->sun_path))")
        return TEST_FAIL;
    }
    return TEST_PASS;
}
static int test_make_url() {
    using namespace cloudbus;
    SET_LOG_PREFIX("test_make_url()");
    auto address = config::make_address("tcp://localhost:80\n");
    if(address.index() != config::URL){
        ERROR_LOG("address.index() != config::URL");
        return TEST_FAIL;
    }
    auto&[host, protocol] = std::get<config::URL>(address);
    if(protocol != "TCP") {
        ERROR_LOG("protocol != \"TCP\"");
        return TEST_FAIL;
    }
    if(host != "localhost:80") {
        ERROR_LOG("host != \"localhost:80\"");
        return TEST_FAIL;
    }
    return TEST_PASS;
}
int main(int argc, char **argv) {
    std::cout << "================================= TEST CONFIG ==================================" << std::endl;
    EXEC_TEST(test_ingest_config);
    EXEC_TEST(test_make_ipv4_address);
    EXEC_TEST(test_make_ipv6_address);
    EXEC_TEST(test_make_unix_address);
    EXEC_TEST(test_make_url);
    std::cout << "================================================================================" << std::endl;
    return TEST_PASS;
}
