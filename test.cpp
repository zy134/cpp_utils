#include "utils.h"
#include "format.h"
#include "Log.h"
#include <string_view>

int main() {
    auto str1 = utils::format("Hello");
    auto str2 = utils::format("Hello {} {} Hello", 1, 2);
    auto str3 = utils::format("Hello {} {} Hello", 1.0, 2.2);
    auto str4 = utils::format("Hello {} {} Hello", "str", "str");
    std::cout << str1 << std::endl;
    std::cout << str2 << std::endl;
    std::cout << str3 << std::endl;
    std::cout << str4 << std::endl;

    std::string_view line = "hello world\n";
    utils::write_log_line(line);
    utils::write_log_line(line);
    utils::write_log_line(line);
    utils::write_log_line(line);
    utils::write_log_line(line);
    utils::write_log_line(line);
    return 0;
}
