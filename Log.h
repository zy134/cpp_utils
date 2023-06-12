#pragma once
#if defined (__clang__)
#pragma clang diagnostic ignored "-Wformat-security"
#endif

#include <array>
#include <cstdarg>
#include <string_view>

namespace utils {

enum class LogLevel {
    Version = 0,
    Debug,
    Info,
    Warning,
    Error,
};

namespace detail {

class LogBuffer;
class LogServer;

void format_log_line(LogLevel level, std::string_view);

constexpr int TransLogLevelToInt(LogLevel level) {
    return static_cast<int>(level);
}

} // namespace detail


// Default log file path.
#ifndef DEFAULT_LOG_PATH
#define DEFAULT_LOG_PATH "/home/zy134/test/LogServer"
#endif
// Default log line length.
#ifndef LOG_MAX_LINE_SIZE
#define LOG_MAX_LINE_SIZE 256
#endif
// Default log file size.
#ifndef LOG_MAX_FILE_SIZE
#define LOG_MAX_FILE_SIZE (1 << 20)
#endif

#ifndef DEFAULT_LOG_LEVEL
#define DEFAULT_LOG_LEVEL 2
#endif

#define LOG_VER(fmt, ...)                                                       \
    if constexpr (static_cast<int>(LogLevel::Version) >= DEFAULT_LOG_LEVEL) {   \
        do {                                                                    \
            std::string_view tmpLogFmt = fmt;                                   \
            std::array<char, LOG_MAX_LINE_SIZE> tmpLogLineBuf;                  \
            snprintf(tmpLogLineBuf.data(), tmpLogLineBuf.size()                 \
                    , tmpLogFmt.data(), ##__VA_ARGS__);                         \
            detail::format_log_line(LogLevel::Version, tmpLogLineBuf.data());   \
        } while(0);                                                             \
    }

#define LOG_DEBUG(fmt, ...)                                                     \
    if constexpr (static_cast<int>(LogLevel::Debug) >= DEFAULT_LOG_LEVEL) {     \
        do {                                                                    \
            std::string_view tmpLogFmt = fmt;                                   \
            std::array<char, LOG_MAX_LINE_SIZE> tmpLogLineBuf;                  \
            snprintf(tmpLogLineBuf.data(), tmpLogLineBuf.size()                 \
                    , tmpLogFmt.data(), ##__VA_ARGS__);                         \
            detail::format_log_line(LogLevel::Debug, tmpLogLineBuf.data());     \
        } while(0);                                                             \
    }

#define LOG_INFO(fmt, ...)                                                      \
    if constexpr (static_cast<int>(LogLevel::Info) >= DEFAULT_LOG_LEVEL) {      \
        do {                                                                    \
            std::string_view tmpLogFmt = fmt;                                   \
            std::array<char, LOG_MAX_LINE_SIZE> tmpLogLineBuf;                  \
            snprintf(tmpLogLineBuf.data(), tmpLogLineBuf.size()                 \
                    , tmpLogFmt.data(), ##__VA_ARGS__);                         \
            detail::format_log_line(LogLevel::Info, tmpLogLineBuf.data());      \
        } while(0);                                                             \
    }


#define LOG_WARN(fmt, ...)                                                      \
    if constexpr (static_cast<int>(LogLevel::Warning) >= DEFAULT_LOG_LEVEL) {   \
        do {                                                                    \
            std::string_view tmpLogFmt = fmt;                                   \
            std::array<char, LOG_MAX_LINE_SIZE> tmpLogLineBuf;                  \
            snprintf(tmpLogLineBuf.data(), tmpLogLineBuf.size()                 \
                    , tmpLogFmt.data(), ##__VA_ARGS__);                         \
            detail::format_log_line(LogLevel::Warning, tmpLogLineBuf.data());   \
        } while(0);                                                             \
    }

#define LOG_ERR(fmt, ...)                                                       \
    if constexpr (static_cast<int>(LogLevel::Error) >= DEFAULT_LOG_LEVEL) {     \
        do {                                                                    \
            std::string_view tmpLogFmt = fmt;                                   \
            std::array<char, LOG_MAX_LINE_SIZE> tmpLogLineBuf;                  \
            snprintf(tmpLogLineBuf.data(), tmpLogLineBuf.size()                 \
                    , tmpLogFmt.data(), ##__VA_ARGS__);                         \
            detail::format_log_line(LogLevel::Error, tmpLogLineBuf.data());     \
        } while(0);                                                             \
    }

}// namespace utils
