#include "Backtrace.h"
#include <cstdlib>
#include <vector>

extern "C" {
#include <execinfo.h>
#include <cxxabi.h>
#include <stdlib.h>
#include <sys/types.h>
}

#include <string>
#include <array>

namespace utils {

std::vector<std::string> getBacktrace() {
    std::vector<std::string> result;

    std::array<void *, MAX_BACKTRACE_DEPTH> backtraceBuffer;

    auto backtraceSize = ::backtrace(backtraceBuffer.data(), backtraceBuffer.size());
    if (backtraceSize < 0) {
        printf("Can't get backtrace because %d!", backtraceSize);
        return result;
    }

    auto* backtraceStrings = ::backtrace_symbols(backtraceBuffer.data(), backtraceSize);
    if (backtraceStrings == nullptr) {
        printf("Can't get backtrace symbols!");
        return result;
    }

    for (int i = 1; i < backtraceSize; ++i) { // skipping the 0-th, which is this function
        // https://panthema.net/2008/0901-stacktrace-demangled/
        // bin/exception_test(_ZN3Bar4testEv+0x79) [0x401909]
        std::string backtraceLine;
#if 1
        char* left_par = nullptr;
        char* plus = nullptr;
        for (char* p = backtraceStrings[i]; *p != '\0'; ++p) {
            if (*p == '(') {
                left_par = p;
            } else if (*p == '+') {
                plus = p;
            }
        }

        if (left_par != nullptr && plus != nullptr) {
            *plus = '\0';
            int status = 0;
            char* realname = abi::__cxa_demangle(left_par + 1, nullptr, nullptr, &status);
            *plus = '+';
            if (status == 0) {
                backtraceLine.append(backtraceStrings[i], left_par + 1);
                backtraceLine.append(realname);
                backtraceLine.append(plus);
                backtraceLine.push_back('\n');
                result.push_back(std::move(backtraceLine));
            } else {
                // LOG_ERR("[Backtrace] demangled failed because of %d", status);
                backtraceLine.append(backtraceStrings[i]);
                result.push_back(std::move(backtraceLine));
            }
            free(realname);
        }
#endif
    }
    free(backtraceStrings);
    return result;
}

}
