#pragma once
#include <cstddef>
#include <stdexcept>
#include <string>
#include <sstream>
#include <type_traits>
#include <iostream>
#include <utility>
#include <string_view>

namespace utils {

namespace detail {
template <typename T, typename ...Args>
void format_recursion(std::stringstream& ss, std::string_view fmt, T&& cur, Args&&... args) {
    auto left = fmt.find('{');
    auto right = fmt.find('}');
    if (left == std::string_view::npos || right == std::string_view::npos || left > right) {
        throw std::runtime_error {"Error format"};
    }
    if constexpr (std::is_floating_point_v<std::decay_t<T>>) {
        ss.setf(std::stringstream::fixed);
    }
    if constexpr (sizeof...(Args) == 0) {
        ss << fmt.substr(0, left) << cur << fmt.substr(right + 1, fmt.size() - right - 1);
    } else {
        ss << fmt.substr(0, left) << cur;
        fmt = fmt.substr(right + 1, fmt.size() - right - 1);
        format_recursion(ss, fmt, std::forward<Args>(args)...);
    }
}
} // namespace impl

template <typename ...Args>
auto format(std::string_view fmt, Args&&...args) -> std::string {
    std::stringstream ss;
    if constexpr (sizeof...(Args) > 0) {
        detail::format_recursion(ss, fmt, std::forward<Args>(args)...);
    } else {
        ss << fmt;
    }
    return ss.str();
}

} // namespace utils
