#pragma once

#include <memory>
#include <string>
#include "utils.h"

namespace utils {

namespace detail {
class LogBuffer;
class LogServer;
} // namespace detail

enum class LogLevel {
    Version = 0,
    Debug,
    Info,
    Warning,
    Error,
};

constexpr std::string_view DEFAULT_PATH = "/home/zy134/test/LogServer";
constexpr size_t LOG_MAX_LINE_SIZE = 256;
constexpr size_t LOG_MAX_FILE_SIZE = (1 << 20);  // 1GB

// Offer two different type of string input, C type printf and C++ type fotmat.
void format_log_line(LogLevel level, std::string_view);
void printf_log_line(LogLevel level, const char *format, ...);

}// namespace utils
