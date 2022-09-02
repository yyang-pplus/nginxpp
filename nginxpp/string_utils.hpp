#pragma once

#include <algorithm>
#include <cctype>
#include <string>


namespace nginxpp {

[[nodiscard]] static inline auto StartsWith(const std::string_view str,
                                            const std::string_view prefix) noexcept {
    return str.rfind(prefix, 0) == 0;
}

[[nodiscard]] static inline auto ToLower(std::string s) noexcept {
    std::transform(s.cbegin(), s.cend(), s.begin(), [](const auto c) {
        return std::tolower(c);
    });
    return s;
}

} //namespace nginxpp
