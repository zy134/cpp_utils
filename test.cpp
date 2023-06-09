#include "utils.h"
#include "format.h"
#include "Log.h"
#include <string>
#include <thread>
#include <sstream>
#include <chrono>
#include <vector>
#include <vector>

int main() {
    std::vector<std::jthread> threadVec;
    for (int i = 0; i != 5; ++i) {
        auto th = std::jthread([] {
            for (int i = 0; i != 5; ++i) {
                std::stringstream stream = {};
                stream << "Thread:" << std::this_thread::get_id()
                       << " ,timestamp:" << std::chrono::system_clock::now().time_since_epoch().count();
                utils::format_log_line(utils::LogLevel::Debug, stream.str());
            }
        });
        threadVec.emplace_back(std::move(th));
    }
    return 0;
}
