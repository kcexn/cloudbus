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
#include "../src/messages.hpp"
#include <sstream>
using namespace cloudbus;
static int test_cmp_uuid() {
    using namespace messages;
    auto uuid_v4 = make_uuid_v4();
    std::stringstream sv4;
    sv4 << uuid_v4;
    FAIL_IF(uuid_v4 != uuid_v4);
    auto uuid_v7 = make_uuid_v7();
    FAIL_IF(uuid_v4 == uuid_v7);
    FAIL_IF(uuid_v7 != uuid_v7);
    FAIL_IF(uuidcmp_node(&uuid_v7, &uuid_v7));
    std::stringstream sv7;
    sv7 << uuid_v7;
    FAIL_IF(sv4.str() == sv7.str());
    return TEST_PASS;
}
int main(int argc, char **argv) {
    std::cout << "================================= TEST MESSAGES ================================" << std::endl;
    EXEC_TEST(test_cmp_uuid);
    std::cout << "================================================================================" << std::endl;
    return TEST_PASS;
}