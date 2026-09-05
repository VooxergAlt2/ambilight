#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ambilight {

// Fixed-memory latency histogram for realtime diagnostics.
//
// Values are stored in coarse upper-bound buckets so observation is O(1),
// allocation-free, and safe to keep in the render path.
class LatencyHistogram {
public:
    static constexpr std::array<std::uint32_t, 10> kUpperBoundsUs = {
        250,
        500,
        1000,
        2000,
        4000,
        8000,
        16000,
        32000,
        64000,
        128000
    };

    void observe(std::uint64_t valueUs) {
        ++samples_;

        if (valueUs > maxObservedUs_) {
            maxObservedUs_ = valueUs;
        }

        for (std::size_t index = 0; index < kUpperBoundsUs.size(); ++index) {
            if (valueUs <= kUpperBoundsUs[index]) {
                ++bins_[index];
                return;
            }
        }

        ++overflow_;
    }

    std::uint32_t percentileUpperBoundUs(std::uint8_t percentile) const {
        if (samples_ == 0 || percentile == 0) {
            return 0;
        }

        if (percentile > 100) {
            percentile = 100;
        }

        const std::uint64_t target =
            (samples_ * percentile + 99) / 100;

        std::uint64_t cumulative = 0;
        for (std::size_t index = 0; index < bins_.size(); ++index) {
            cumulative += bins_[index];
            if (cumulative >= target) {
                return kUpperBoundsUs[index];
            }
        }

        // The requested percentile landed in the overflow bucket.
        // Return the largest represented upper bound and expose overflow()
        // separately so diagnostics can show that the real value was larger.
        return kUpperBoundsUs.back();
    }

    std::uint64_t samples() const { return samples_; }
    std::uint64_t overflow() const { return overflow_; }
    std::uint64_t maxObservedUs() const { return maxObservedUs_; }

private:
    std::array<std::uint64_t, kUpperBoundsUs.size()> bins_{};
    std::uint64_t samples_ = 0;
    std::uint64_t overflow_ = 0;
    std::uint64_t maxObservedUs_ = 0;
};

} // namespace ambilight
