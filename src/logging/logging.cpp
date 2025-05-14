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
#include "logging.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>
namespace cloudbus {
    std::ostream* Logger::s_output_stream_ = &std::cout;
    namespace {
        template<class ToDuration, class Rep, class Period>
        static ToDuration duration_cast(
            const std::chrono::duration<Rep, Period>& duration
        ){
            return std::chrono::duration_cast<ToDuration>(
                duration
            );
        }
        template<class Rep, class Period>
        static std::chrono::milliseconds to_milliseconds(
            const std::chrono::duration<Rep, Period>& duration
        ){
            return ::cloudbus::duration_cast<std::chrono::milliseconds>(
                duration
            );
        }
    }
    void Logger::setLevel(Level level) {
        level_.store(level, std::memory_order_relaxed);
        info("Log level set to " + levelToString(level));
    }
    Logger::Level Logger::getLevel() const {
        return level_.load(std::memory_order_relaxed);
    }
    void Logger::log(Level level, const std::string_view& message) {
        if(level <= getLevel()) {
            std::lock_guard<std::mutex> lk(log_mutex_);
            *s_output_stream_ << "<" << static_cast<int>(level) << "> "
                    << "<" << levelToString(level) << "> "
                    << "<" << getCurrentTimestamp() << "> "
                    << message << '\n';
        }
    }
    void Logger::flush() {
        s_output_stream_->flush();
    }
    void Logger::emergency(const std::string_view& message){
        log(Level::EMERGENCY, message);
    }
    void Logger::alert(const std::string_view& message){
        log(Level::ALERT, message);
    }
    void Logger::critical(const std::string_view& message){
        log(Level::CRITICAL, message);
    }
    void Logger::error(const std::string_view& message){
        log(Level::ERROR, message);
    }
    void Logger::warn(const std::string_view& message){
        log(Level::WARNING, message);
    }
    void Logger::notice(const std::string_view& message){
        log(Level::NOTICE, message);
    }
    void Logger::info(const std::string_view& message){
        log(Level::INFORMATIONAL, message);
    }
    void Logger::debug(const std::string_view& message){
        log(Level::DEBUG, message);
    }
    std::string Logger::getCurrentTimestamp() {
        const auto now = std::chrono::system_clock::now();
        const auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        std::tm tm_buf{};

        localtime_r(&in_time_t, &tm_buf);
        ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");

        // Add milliseconds for more precision
        auto ms = to_milliseconds(now.time_since_epoch()) % 1000;
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        
        return ss.str();
    }
    std::string Logger::levelToString(Level level) {
        switch (level) {
            case Level::EMERGENCY:
                return "EMERGENCY";
            case Level::ALERT:
                return "ALERT    "; // Padded for alignment.
            case Level::CRITICAL:
                return "CRITICAL ";
            case Level::ERROR:
                return "ERROR    ";
            case Level::WARNING:
                return "WARNING  ";
            case Level::NOTICE:
                return "NOTICE   ";
            case Level::INFORMATIONAL:
                return "INFO     ";
            case Level::DEBUG:
                return "DEBUG    ";
            default: {
                std::stringstream ss("Level ");
                ss << static_cast<int>(level);
                return ss.str();
            }
        }
    }
}
