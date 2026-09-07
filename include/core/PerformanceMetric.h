#pragma once

#include <cstdint>

#include "core/LatencyHistogram.h"

namespace ambilight {

// Fixed-memory timing accumulator for hot-path instrumentation.
//
// Observation is allocation-free and O(1) apart from the small fixed
// LatencyHistogram bucket scan. The metric deliberately keeps exact last,
// max and integer mean values while percentiles remain coarse upper bounds.
// This avoids serial logging or dynamic allocation in realtime paths.
class PerformanceMetric {
public:
    void observe(std::uint64_t elapsedUs) {
        lastUs_ = elapsedUs;

        if (elapsedUs > maxUs_) {
            maxUs_ = elapsedUs;
        }

        totalUs_ += elapsedUs;
        ++samples_;

        histogram_.observe(elapsedUs);
    }

    void reset() {
        *this = {};
    }

    std::uint64_t samples() const {
        return samples_;
    }

    std::uint64_t lastUs() const {
        return lastUs_;
    }

    std::uint64_t maxUs() const {
        return maxUs_;
    }

    std::uint64_t meanUs() const {
        return samples_ == 0
            ? 0
            : totalUs_ / samples_;
    }

    std::uint32_t percentileUpperBoundUs(
        std::uint8_t percentile) const {

        return histogram_.
            percentileUpperBoundUs(
                percentile);
    }

    std::uint64_t percentileOverflow() const {
        return histogram_.overflow();
    }

private:
    LatencyHistogram histogram_{};

    std::uint64_t samples_ = 0;
    std::uint64_t totalUs_ = 0;
    std::uint64_t lastUs_ = 0;
    std::uint64_t maxUs_ = 0;
};

} // namespace ambilight
