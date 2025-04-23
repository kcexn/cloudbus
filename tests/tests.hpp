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
#pragma once
#ifndef CLOUDBUS_TESTS
#define CLOUDBUS_TESTS
#include <string>
#include <iostream>

/* make check status codes */
#define TEST_PASS 0
#define TEST_SKIP 77
#define TEST_ERROR 99
#define TEST_FAIL -1

#define STRSTATUS(STATUS) (                 \
    STATUS==TEST_PASS ? "PASS" :            \
    STATUS==TEST_SKIP ? "SKIP" :            \
    STATUS==TEST_ERROR ? "ERROR" : "FAIL"   \
)

static int status = TEST_PASS;
#define EXEC_TEST(FN) {         \
    if(status != TEST_SKIP)     \
        status=FN();            \
    std::cout << #FN "():"      \
        << STRSTATUS(status)    \
        << std::endl;           \
    switch(status) {            \
        case TEST_SKIP:         \
            status = TEST_PASS; \
        case TEST_PASS:         \
            break;              \
        default:                \
            return status;      \
    }                           \
}
#define SKIP \
    status = TEST_SKIP;

#define ERROR_LOG(MSG) {    \
    std::cerr << __FILE__   \
        << ':' << __LINE__  \
        << ':' << MSG       \
        << std::endl;       \
}
#define FAIL(MSG) {     \
    ERROR_LOG(MSG);     \
    return TEST_FAIL;   \
}
#define FAIL_IF(COND) { \
    if(COND)            \
        FAIL(#COND);    \
}

#endif
