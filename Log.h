#pragma once

#if defined (__clang__)
#pragma clang diagnostic ignored "-Wformat-security"
#endif

#if defined (__GNUC__)
#pragma GCC diagnostic ignored "-Wformat-security"
#endif

#include <array>
#include <string_view>

extern "C" {
#include <stdio.h>
}

namespace utils {

enum class LogLevel {
    Version = 0,
    Debug,
    Info,
    Warning,
    Error,
    Fatal,
};

namespace detail {

class LogBuffer;
class LogServer;

// Exception should be deal with in internal module of Log.
void format_log_line(LogLevel level, std::string_view fmt, std::string_view tag) noexcept;

constexpr int TransLogLevelToInt(LogLevel level) {
    return static_cast<int>(level);
}

} // namespace detail


// Default log file path.
#ifndef DEFAULT_LOG_PATH
#define DEFAULT_LOG_PATH "/home/zy134/test/ChatServer/logs"
#endif
// Default log line length.
#ifndef LOG_MAX_LINE_SIZE
#define LOG_MAX_LINE_SIZE 512
#endif
// Default log file size.
#ifndef LOG_MAX_FILE_SIZE
#define LOG_MAX_FILE_SIZE (1 << 20)
#endif

#ifndef DEFAULT_LOG_LEVEL
#define DEFAULT_LOG_LEVEL 1
#endif

#define LOG_VER(fmt, ...)                                                       \
    if constexpr (static_cast<int>(LogLevel::Version) >= DEFAULT_LOG_LEVEL) {   \
        do {                                                                    \
            std::string_view tmpLogFmt = fmt;                                   \
            std::array<char, LOG_MAX_LINE_SIZE> tmpLogLineBuf;                  \
            snprintf(tmpLogLineBuf.data(), tmpLogLineBuf.size()                 \
                    , tmpLogFmt.data(), ##__VA_ARGS__);                         \
            detail::format_log_line(LogLevel::Version, tmpLogLineBuf.data(), TAG);  \
        } while(0);                                                             \
    }

#define LOG_DEBUG(fmt, ...)                                                     \
    if constexpr (static_cast<int>(LogLevel::Debug) >= DEFAULT_LOG_LEVEL) {     \
        do {                                                                    \
            std::string_view tmpLogFmt = fmt;                                   \
            std::array<char, LOG_MAX_LINE_SIZE> tmpLogLineBuf;                  \
            snprintf(tmpLogLineBuf.data(), tmpLogLineBuf.size()                 \
                    , tmpLogFmt.data(), ##__VA_ARGS__);                         \
            detail::format_log_line(LogLevel::Debug, tmpLogLineBuf.data(), TAG);\
        } while(0);                                                             \
    }

#define LOG_INFO(fmt, ...)                                                      \
    if constexpr (static_cast<int>(LogLevel::Info) >= DEFAULT_LOG_LEVEL) {      \
        do {                                                                    \
            std::string_view tmpLogFmt = fmt;                                   \
            std::array<char, LOG_MAX_LINE_SIZE> tmpLogLineBuf;                  \
            snprintf(tmpLogLineBuf.data(), tmpLogLineBuf.size()                 \
                    , tmpLogFmt.data(), ##__VA_ARGS__);                         \
            detail::format_log_line(LogLevel::Info, tmpLogLineBuf.data(), TAG); \
        } while(0);                                                             \
    }


#define LOG_WARN(fmt, ...)                                                      \
    if constexpr (static_cast<int>(LogLevel::Warning) >= DEFAULT_LOG_LEVEL) {   \
        do {                                                                    \
            std::string_view tmpLogFmt = fmt;                                   \
            std::array<char, LOG_MAX_LINE_SIZE> tmpLogLineBuf;                  \
            snprintf(tmpLogLineBuf.data(), tmpLogLineBuf.size()                 \
                    , tmpLogFmt.data(), ##__VA_ARGS__);                         \
            detail::format_log_line(LogLevel::Warning, tmpLogLineBuf.data(), TAG);  \
        } while(0);                                                             \
    }

#define LOG_ERR(fmt, ...)                                                       \
    if constexpr (static_cast<int>(LogLevel::Error) >= DEFAULT_LOG_LEVEL) {     \
        do {                                                                    \
            std::string_view tmpLogFmt = fmt;                                   \
            std::array<char, LOG_MAX_LINE_SIZE> tmpLogLineBuf;                  \
            snprintf(tmpLogLineBuf.data(), tmpLogLineBuf.size()                 \
                    , tmpLogFmt.data(), ##__VA_ARGS__);                         \
            detail::format_log_line(LogLevel::Error, tmpLogLineBuf.data(), TAG);\
        } while(0);                                                             \
    }

#define LOG_FATAL(fmt, ...)                                                     \
    if constexpr (static_cast<int>(LogLevel::Fatal) >= DEFAULT_LOG_LEVEL) {     \
        do {                                                                    \
            std::string_view tmpLogFmt = fmt;                                   \
            std::array<char, LOG_MAX_LINE_SIZE> tmpLogLineBuf;                  \
            snprintf(tmpLogLineBuf.data(), tmpLogLineBuf.size()                 \
                    , tmpLogFmt.data(), ##__VA_ARGS__);                         \
            detail::format_log_line(LogLevel::Fatal, tmpLogLineBuf.data(), TAG);\
        } while(0);                                                             \
    }

void assertTrue(bool cond, std::string_view msg);

void printBacktrace();

}// namespace utils
