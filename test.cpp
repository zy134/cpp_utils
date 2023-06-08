#include "utils.h"
#include "format.h"
#include "Log.h"

int main() {
    std::string line = "hello world";
    utils::format_log_line(utils::LogLevel::Debug, line);
    utils::format_log_line(utils::LogLevel::Debug, line);
    utils::format_log_line(utils::LogLevel::Debug, line);
    utils::format_log_line(utils::LogLevel::Debug, line);
    utils::format_log_line(utils::LogLevel::Debug, line);
    utils::format_log_line(utils::LogLevel::Debug, line);
    utils::format_log_line(utils::LogLevel::Debug, line);
    utils::format_log_line(utils::LogLevel::Debug, line);
    return 0;
}
