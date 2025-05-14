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
#include "../logging.hpp"
#include "options.hpp"
#include <algorithm>
#include <iostream>
#include <cstdlib>
#include <cstring>
namespace cloudbus {
    namespace options {
        namespace {
            static void throw_system_error(const std::string& what) {
                throw std::system_error(
                    std::error_code(errno, std::system_category()),
                    what
                );
            }
            static int set_log_level(std::string_view opt) {
                if(!opt.empty()) {
                    auto& logger = Logger::getInstance();
                    std::string level{opt};
                    auto begin = level.begin(), end = level.end();
                    std::transform(
                        begin,
                        end,
                        begin,
                        [](unsigned char c){
                            return std::tolower(c);
                        }
                    );
                    if(level == "emergency" || level == "emerg" || level == "0") {
                        logger.setLevel(Logger::Level::EMERGENCY);
                    } else if (level == "alert" || level == "1") {
                        logger.setLevel(Logger::Level::ALERT);
                    } else if (level == "critical" || level == "crit" || level == "2") {
                        logger.setLevel(Logger::Level::CRITICAL);
                    } else if (level == "error" || level == "err" || level == "3") {
                        logger.setLevel(Logger::Level::ERROR);
                    } else if (level == "warning" || level == "warn" || level == "4") {
                        logger.setLevel(Logger::Level::WARNING);
                    } else if (level == "notice" || level == "5") {
                        logger.setLevel(Logger::Level::NOTICE);
                    } else if (level == "informational" || level == "info" || level == "6") {
                        logger.setLevel(Logger::Level::INFORMATIONAL);
                    } else if (level == "debug" || level == "7") {
                        logger.setLevel(Logger::Level::DEBUG);
                    } else {
                        logger.error("Unrecognized log-level: " +
                            std::string(opt) + 
                            ". Using default log-level: WARN.");
                    }
                }
                return 0;
            }
            static int set_config_file(std::string_view opt) {
                if(!opt.empty()) {
                    if(setenv("CONFIG_PATH", std::string(opt).c_str(), 1))
                        throw_system_error("Unable to set CONFIG_PATH");
                }
                return 0;
            }
            static int help(std::string_view component) {
                std::cout << "Usage: " << component
                    << " [OPTION]...\n"
                    << "Run " << component << ".\n\n"
                    << "  -f, --file         path to configuration file.\n"
                    << "  -l, --log-level    set the log level.\n"
                    << "      --help         display this help and exit.\n";
                return -1;
            }
            static int invalid(std::string_view component, std::string_view opt) {
                auto begin = opt.begin(), end = opt.end();
                while(begin != end && *begin == '-')
                    ++begin;
                std::cout << component << ": invalid option -- " 
                    << "'" << std::string_view(&*begin, std::distance(begin, end)) << "'\n"
                    << "Try '" << component << " --help' for more information.\n";
                return -1;
            }
        }
        int parse(int argc, char *argv[]) {
            for(int i = 1; i < argc; ++i) {
                if(!strcmp(argv[i], "-f") || !strcmp(argv[i], "--file")) {
                    if(++i < argc)
                        set_config_file(argv[i]);
                } else if(!strcmp(argv[i], "-l") || !strcmp(argv[i], "--log-level")) {
                    if(++i < argc)
                        set_log_level(argv[i]);
                } else if(!strcmp(argv[i], "--help")) {
                    return help(argv[0]);
                } else {
                    return invalid(argv[0], argv[i]);
                }
            }
            return 0;
        }
    }
}
