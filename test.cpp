#include "utils.h"
#include "format.h"
#include "Log.h"
#include <string>
#include <thread>
#include <sstream>
#include <chrono>
#include <vector>
#include <vector>

using namespace utils;

int main() {
    std::vector<std::jthread> threadVec;
    for (int i = 0; i != 5; ++i) {
        auto th = std::jthread([] {
            for (int i = 0; i != 5; ++i) {
                std::stringstream stream = {};
                stream << "Thread:" << std::this_thread::get_id()
                       << " ,timestamp:" << std::chrono::system_clock::now().time_since_epoch().count();
                auto str = stream.str();
                LOG_DEBUG(str);
                LOG_INFO(str);
                LOG_WARN(str);
            }
        });
        threadVec.emplace_back(std::move(th));
    }
    return 0;
}
