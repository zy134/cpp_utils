#include <string>
#include <vector>

namespace utils {

constexpr uint32_t MAX_BACKTRACE_DEPTH = 16;

void PrintBacktrace();

std::vector<std::string> GetBacktrace();

}
