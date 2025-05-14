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
#include "../src/logging.hpp"
#include <sstream>
using namespace cloudbus;
static int test_logger_singleton() {
    Logger& logger1 = Logger::getInstance();
    Logger& logger2 = Logger::getInstance();
    FAIL_IF(&logger1 != &logger2);
    return TEST_PASS;
}
static int test_logger_default_level() {
    Logger& logger = Logger::getInstance();
    FAIL_IF(logger.getLevel() != Logger::Level::WARNING);
    return TEST_PASS;
}
static int test_logger_set_and_get_level() {
    Logger& logger = Logger::getInstance();

    logger.setLevel(Logger::Level::DEBUG);
    FAIL_IF(logger.getLevel() != Logger::Level::DEBUG);

    logger.setLevel(Logger::Level::INFORMATIONAL);
    FAIL_IF(logger.getLevel() != Logger::Level::INFORMATIONAL);

    logger.setLevel(Logger::Level::ERROR);
    FAIL_IF(logger.getLevel() != Logger::Level::ERROR);

    logger.setLevel(Logger::Level::WARNING);
    FAIL_IF(logger.getLevel() != Logger::Level::WARNING);

    return TEST_PASS;
}
static int test_logger_filtering() {
    Logger& logger = Logger::getInstance();
    std::stringstream test_output;
    Logger::setOutputStream(test_output);

    logger.debug("debug_msg_at_warn_level"); // Should not appear
    logger.info("info_msg_at_warn_level");   // Should not appear
    logger.warn("warn_msg_at_warn_level");   // Should appear
    logger.error("error_msg_at_warn_level"); // Should appear

    std::string output = test_output.str();
    FAIL_IF(output.find("debug_msg_at_warn_level") != std::string::npos);
    FAIL_IF(output.find("info_msg_at_warn_level") != std::string::npos);
    FAIL_IF(output.find("warn_msg_at_warn_level") == std::string::npos);
    FAIL_IF(output.find("error_msg_at_warn_level") == std::string::npos);

    // Case 2: Log level WARNING
    logger.setLevel(Logger::Level::INFORMATIONAL); // This will add its own log line
    test_output.str(""); // Clear stream *after* setLevel's own log
    
    logger.debug("debug_msg_at_info_level"); // Should not appear
    logger.info("info_msg_at_info_level");   // Should appear
    logger.warn("warn_msg_at_info_level");   // Should appear
    logger.error("error_msg_at_info_level"); // Should appear

    output = test_output.str();
    FAIL_IF(output.find("debug_msg_at_info_level") != std::string::npos);
    FAIL_IF(output.find("info_msg_at_info_level") == std::string::npos);
    FAIL_IF(output.find("warn_msg_at_info_level") == std::string::npos);
    FAIL_IF(output.find("error_msg_at_info_level") == std::string::npos);

    // Case 3: Log level DEBUG (all should appear)
    logger.setLevel(Logger::Level::DEBUG);
    test_output.str(""); // Clear stream *after* setLevel's own log

    logger.debug("debug_msg_at_debug_level");
    logger.info("info_msg_at_debug_level");
    logger.warn("warn_msg_at_debug_level");
    logger.error("error_msg_at_debug_level");

    output = test_output.str();
    FAIL_IF(output.find("debug_msg_at_debug_level") == std::string::npos);
    FAIL_IF(output.find("info_msg_at_debug_level") == std::string::npos);
    FAIL_IF(output.find("warn_msg_at_debug_level") == std::string::npos);
    FAIL_IF(output.find("error_msg_at_debug_level") == std::string::npos);

    return TEST_PASS;
}
int main(int argc, char **argv) {
    std::cout << "================================ TEST LOGGING ==================================" << std::endl;
    EXEC_TEST(test_logger_singleton);
    EXEC_TEST(test_logger_default_level);
    EXEC_TEST(test_logger_set_and_get_level);
    EXEC_TEST(test_logger_filtering);
    std::cout << "================================================================================" << std::endl;
    return TEST_PASS;
}
