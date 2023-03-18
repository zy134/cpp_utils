#include <cstddef>
#include <string>
#include <sstream>
#include <type_traits>
#include <iostream>
#include <utility>

namespace utils {

namespace impl {
template <typename T, typename ...Args>
void format_recursion(std::stringstream& ss, const std::string& origin_str, std::string::size_type start_pos, T&& cur, Args&&... args) {
    if constexpr (sizeof...(Args) == 0) {
        if (auto end_pos = origin_str.find('{', start_pos); end_pos == std::string::npos) {
            std::cerr << "Too much arguments for format!" << std::endl;
            ss << origin_str.substr(start_pos, origin_str.size() - start_pos);
        } else {
            if constexpr (std::is_floating_point_v<std::decay_t<T>>) {
                ss.setf(std::stringstream::fixed);
            }
            ss << origin_str.substr(start_pos, end_pos - start_pos) << cur;
            if (end_pos + 2 < origin_str.size())
                ss << origin_str.substr(end_pos + 2, origin_str.size() - end_pos - 2);
        }

    } else {
        if (auto end_pos = origin_str.find('{', start_pos); end_pos == std::string::npos) {
            std::cerr << "Too much arguments for format!" << std::endl;
            ss << origin_str.substr(start_pos, origin_str.size() - start_pos);
        } else {
            if constexpr (std::is_floating_point_v<std::decay_t<T>>) {
                ss.setf(std::stringstream::fixed);
            }
            ss << origin_str.substr(start_pos, end_pos - start_pos) << cur;
                format_recursion(ss, origin_str, end_pos + 2, std::forward<Args>(args)...);
        }
    }
}
} // namespace impl

template <typename ...Args>
auto format(const std::string& str, Args&&...args) -> std::string {
    if constexpr (sizeof...(Args) > 0) {
        std::stringstream ss;
        impl::format_recursion(ss, str, 0, std::forward<Args>(args)...);
        return ss.str();
    } else {
        return str;
    }
}

} // namespace utils
