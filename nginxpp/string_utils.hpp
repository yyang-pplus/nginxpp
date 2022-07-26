#pragma once

#include <algorithm>
#include <cctype>
#include <string>


namespace nginxpp {

[[nodiscard]] static inline auto ToLower(std::string s) {
    std::transform(s.cbegin(), s.cend(), s.begin(), [](const auto c) {
        return std::tolower(c);
    });
    return s;
}

} //namespace nginxpp
