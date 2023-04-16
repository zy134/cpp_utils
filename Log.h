#pragma once

#include <memory>
#include <string_view>
#include "utils.h"

namespace utils {

namespace detail {

class LogServer;

class LogBuffer;

} // namespace detail

enum class LogLevel {
    Version,
    Debug,
    Info,
    Warning,
    Error,
};

class LogClient {
public:
    DISABLE_COPY(LogClient);
    DISABLE_MOVE(LogClient);

    LogClient();
    ~LogClient();
    void write(std::string fmt, LogLevel level);

private:
    std::shared_ptr<detail::LogServer> mpLogServer;
};

} // namespace utils
