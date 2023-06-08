#pragma once

#include <memory>
#include <string_view>
#include "utils.h"

namespace utils {

void write_log_line(std::string_view fmt);

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


} // namespace utils
