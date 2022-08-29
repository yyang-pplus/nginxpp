#pragma once

#include <chrono>


namespace nginxpp {

template<typename DestinationTimePoint, typename SourceTimePoint>
static inline constexpr DestinationTimePoint ClockCast(const SourceTimePoint &tp) {
    const auto source_now = SourceTimePoint::clock::now();
    const auto destination_now = DestinationTimePoint::clock::now();
    return destination_now + (tp - source_now);
}

} //namespace nginxpp
