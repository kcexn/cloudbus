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
    config::configuration conf;
    std::stringstream ss{
        "[Cloudbus]\n"
        "[Test Service]\n"
        "bind=unix:///var/run/test.sock\n"
        "backend=unix:///var/run/backend.sock\n"
    };
    ss >> conf;
    for(auto& section: conf.sections()) {
        FAIL_IF(section.heading != "Cloudbus" && section.heading != "Test Service");
        FAIL_IF(section.heading == "Cloudbus" && !section.config.empty());
        FAIL_IF(section.heading == "Test Service" && section.config.empty());
        if (section.heading== "Test Service") {
            for(auto&[k,v]: section.config) {
                FAIL_IF(k != "bind" && k != "backend");
                FAIL_IF(k == "bind" && v != "unix:///var/run/test.sock");
                FAIL_IF(k == "backend" && v != "unix:///var/run/backend.sock");
            }
        }
    }
    return TEST_PASS;
}
static int test_make_ipv4_address() {
    using namespace cloudbus;
    auto address = config::make_address("tcp://192.168.1.1:80");
    FAIL_IF(address.index() != config::SOCKADDR);

    auto&[protocol, addr_storage, len] = std::get<config::SOCKADDR>(address);
    FAIL_IF(protocol != "TCP");
    FAIL_IF(addr_storage.ss_family != AF_INET);

    auto *in_addr = reinterpret_cast<struct sockaddr_in*>(&addr_storage);
    FAIL_IF(in_addr->sin_port != htons(80));

    char dst[INET_ADDRSTRLEN];
    std::memset(dst, '\0', sizeof(dst));
    const char *net = inet_ntop(in_addr->sin_family, &in_addr->sin_addr, dst, sizeof(dst));
    FAIL_IF(std::strncmp(net, "192.168.1.1", sizeof(dst)));

    return TEST_PASS;
}
static int test_make_ipv6_address() {
    using namespace cloudbus;
    auto address = config::make_address("tcp://[::1]:80");
    FAIL_IF(address.index() != config::SOCKADDR);

    auto&[protocol, addr_storage, len] = std::get<config::SOCKADDR>(address);
    FAIL_IF(protocol != "TCP");
    FAIL_IF(addr_storage.ss_family != AF_INET6);

    auto *in_addr = reinterpret_cast<struct sockaddr_in6*>(&addr_storage);
    FAIL_IF(in_addr->sin6_port != htons(80));

    char dst[INET6_ADDRSTRLEN];
    std::memset(dst, '\0', sizeof(dst));
    const char *net = inet_ntop(in_addr->sin6_family, &in_addr->sin6_addr, dst, sizeof(dst));
    FAIL_IF(std::strncmp(net, "::1", sizeof(dst)));

    return TEST_PASS;
}
static int test_make_unix_address() {
    using namespace cloudbus;
    auto address = config::make_address("unix:///var/run/test.sock");
    FAIL_IF(address.index() != config::SOCKADDR);

    auto&[protocol, addr_storage, len] = std::get<config::SOCKADDR>(address);
    FAIL_IF(protocol != "UNIX");
    FAIL_IF(addr_storage.ss_family != AF_UNIX);

    auto *un_addr = reinterpret_cast<struct sockaddr_un*>(&addr_storage);
    FAIL_IF(std::strncmp(un_addr->sun_path, "/var/run/test.sock", sizeof(un_addr->sun_path)));

    return TEST_PASS;
}
static int test_make_url() {
    using namespace cloudbus;
    auto address = config::make_address("tcp://localhost:80");
    FAIL_IF(address.index() != config::URL);

    auto&[host, protocol] = std::get<config::URL>(address);
    FAIL_IF(protocol != "TCP");
    FAIL_IF(host != "localhost:80");

    return TEST_PASS;
}
static int test_make_urn() {
    using namespace cloudbus;
    auto address = config::make_address(" urn:localhost:80 ");
    FAIL_IF(address.index()!=config::URI);
    FAIL_IF(std::get<config::URI>(address)!="urn:localhost:80");

    return TEST_PASS;
}
static int test_make_srv() {
    using namespace cloudbus;
    auto address = config::make_address(" srv:_service._tcp.arpa ");
    FAIL_IF(address.index()!=config::URI);
    FAIL_IF(std::get<config::URI>(address)!="srv:_service._tcp.arpa");

    return TEST_PASS;
}
static int test_invalid_uri() {
    using namespace cloudbus;
    auto address = config::make_address("_service._tcp.arpa");
    FAIL_IF(address.index()!=config::URI);
    FAIL_IF(!std::get<config::URI>(address).empty());
    
    return TEST_PASS;
}
int main(int argc, char **argv) {
    std::cout << "================================= TEST CONFIG ==================================" << std::endl;
    EXEC_TEST(test_ingest_config);
    EXEC_TEST(test_make_ipv4_address);
    EXEC_TEST(test_make_ipv6_address);
    EXEC_TEST(test_make_unix_address);
    EXEC_TEST(test_make_url);
    EXEC_TEST(test_make_urn);
    EXEC_TEST(test_make_srv);
    EXEC_TEST(test_invalid_uri);
    std::cout << "================================================================================" << std::endl;
    return TEST_PASS;
}
