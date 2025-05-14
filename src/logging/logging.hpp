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
#include <atomic>
#include <iostream>
#include <string>
#include <string_view>
#include <mutex>

#pragma once
#ifndef CLOUDBUS_LOGGING
#define CLOUDBUS_LOGGING
namespace cloudbus {
    // Logger Class
    class Logger {
    public:
        static void setOutputStream(std::ostream& stream) {
            s_output_stream_ = &stream;
        }
        static void resetOutputStream() {
            s_output_stream_ = &std::cout; // Default back to std::cout
        }

        enum class Level {
            EMERGENCY,
            ALERT,
            CRITICAL,
            ERROR,
            WARNING,
            NOTICE,
            INFORMATIONAL,
            DEBUG
        };
        // Static method to get the singleton instance
        static Logger& getInstance() {
            // Meyer's Singleton: C++11 guarantees thread-safe initialization of local static variables.
            static Logger instance;
            return instance;
        }

        void setLevel(Level level);
        Level getLevel() const;
        void log(Level level, const std::string_view& message);
        void flush();

        void emergency(const std::string_view& message);
        void alert(const std::string_view& message);
        void critical(const std::string_view& message);
        void error(const std::string_view& message);
        void warn(const std::string_view& message);
        void notice(const std::string_view& message);
        void info(const std::string_view& message);
        void debug(const std::string_view& message);

        Logger(const Logger& other) = delete;
        Logger& operator=(const Logger& other) = delete;
        Logger(Logger&& other) = delete;
        Logger& operator=(Logger&& other) = delete;

    private:
        Logger() : level_(Level::WARNING) {}
        ~Logger() = default;

        std::string getCurrentTimestamp();
        std::string levelToString(Level level);  

        std::mutex log_mutex_;
        std::atomic<Level> level_;
        static std::ostream *s_output_stream_;
    };
}
#endif
