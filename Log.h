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
    Version,
    Debug,
    Info,
    Warning,
    Error,
};

// Offer two different type of string input, C type printf and C++ type fotmat.
void format_log_line(LogLevel level, const std::string&);
void printf_log_line(LogLevel level, const char *format, ...);

}// namespace utils
