#pragma once

#include <chrono>


namespace nginxpp {

template<typename DestinationTimePoint, typename SourceTimePoint>
static inline constexpr DestinationTimePoint ClockCast(const SourceTimePoint &tp) {
    const auto source_now = SourceTimePoint::clock::now();
    const auto destination_now = DestinationTimePoint::clock::now();
    return destination_now + (tp - source_now);
}

static inline auto &operator<<(std::ostream &out,
                               const std::chrono::system_clock::time_point &tp) noexcept {
    constexpr auto *format = "%F %T %Z";

    const auto tt = std::chrono::system_clock::to_time_t(tp);
    const auto *tm = std::gmtime(&tt); //not thread-safe
    if (tm) {
        out << std::put_time(tm, format);
    }

    return out;
}

static inline auto &operator<<(std::ostream &out,
                               const std::filesystem::file_time_type &tp) noexcept {
    return out << ClockCast<std::chrono::system_clock::time_point>(tp);
}

} //namespace nginxpp
