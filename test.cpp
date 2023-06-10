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
                LOG_DEBUG(stream.str());
                LOG_INFO(stream.str());
                LOG_WARN(stream.str());
            }
        });
        threadVec.emplace_back(std::move(th));
    }
    return 0;
}
