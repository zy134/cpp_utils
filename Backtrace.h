#pragma once

#include <string>
#include <vector>

namespace utils {

constexpr uint32_t MAX_BACKTRACE_DEPTH = 16;

std::vector<std::string> getBacktrace();

}
