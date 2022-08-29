#pragma once

#include <sys/stat.h>
#include <sys/types.h>

#include <filesystem>
#include <iomanip>

#include <gsl/gsl>

#include <nginxpp/chrono_utils.hpp>


namespace nginxpp {

[[nodiscard]] static inline long
GetFileSize(const gsl::not_null<gsl::czstring> filename) noexcept {
    struct stat st;

    if (stat(filename, &st) != -1) {
        return st.st_size;
    }

    return -1;
}

struct PathStats {
    std::filesystem::path path;
    std::filesystem::file_time_type modification_time;
    long size = -1;

    explicit PathStats(const std::filesystem::path &p) noexcept :
        path(p), modification_time(last_write_time(p)) {
        if (is_regular_file(p)) {
            size = GetFileSize(path.c_str());
        }
    }
};

[[nodiscard]] static inline auto StartsWith(const std::filesystem::path &p,
                                            const std::filesystem::path &prefix) {
    Expects(p.is_absolute());
    Expects(prefix.is_absolute());

    for (auto p_iter = p.begin(), prefix_iter = prefix.begin(); prefix_iter != prefix.end();
         ++p_iter, ++prefix_iter) {
        if (p_iter == p.end() or *p_iter != *prefix_iter) {
            return false;
        }
    }

    return true;
}

std::ostream &operator<<(std::ostream &out, const std::filesystem::file_time_type &tp) noexcept {
    constexpr auto *format = "%F %T %Z";

    const auto tt = std::chrono::system_clock::to_time_t(
        ClockCast<std::chrono::system_clock::time_point>(tp));
    const auto *tm = std::gmtime(&tt);
    if (tm) {
        out << std::put_time(tm, format);
    }

    return out;
}

} //namespace nginxpp
