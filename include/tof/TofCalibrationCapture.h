#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "tof/TofTypes.h"

namespace ambilight {

struct CalibrationBandSummary {
    bool valid = false;

    std::uint16_t p10Mm = 0;
    std::uint16_t medianMm = 0;
    std::uint16_t p90Mm = 0;

    std::uint16_t medianMadMm = 0;

    std::uint8_t minAccepted = 0;
    std::uint8_t maxAccepted = 0;
};

struct CalibrationCaptureSummary {
    std::uint32_t totalFrames = 0;
    std::uint32_t validFrames = 0;
    std::uint32_t duplicateFrames = 0;
    std::uint32_t overflowFrames = 0;

    CalibrationBandSummary left{};
    CalibrationBandSummary center{};
    CalibrationBandSummary right{};

    std::int16_t medianRightMinusLeftMm = 0;
};

class TofCalibrationCapture {
public:
    static constexpr std::size_t kMaxSamples = 64;
    static constexpr std::uint64_t kDefaultDurationUs = 5000000;

    void start(
        std::uint64_t nowUs,
        std::uint64_t durationUs = kDefaultDurationUs);

    void cancel();

    bool active() const { return active_; }

    bool ingest(const TofGeometrySnapshot& geometry);

    bool expired(std::uint64_t nowUs) const;

    CalibrationCaptureSummary finish();

    std::uint32_t storedSamples() const {
        return storedSamples_;
    }

private:
    struct BandSamples {
        std::array<std::uint16_t, kMaxSamples> distanceMm{};
        std::array<std::uint16_t, kMaxSamples> madMm{};
        std::array<std::uint8_t, kMaxSamples> accepted{};
    };

    static std::uint16_t percentile(
        std::array<std::uint16_t, kMaxSamples> values,
        std::size_t count,
        std::uint8_t percentile);

    static CalibrationBandSummary summarizeBand(
        const BandSamples& samples,
        std::size_t count);

    static std::int16_t summarizeDelta(
        std::array<std::int16_t, kMaxSamples> values,
        std::size_t count);

    bool active_ = false;

    std::uint64_t startedUs_ = 0;
    std::uint64_t durationUs_ = kDefaultDurationUs;

    std::uint32_t lastGeometryGeneration_ = 0;

    std::uint32_t totalFrames_ = 0;
    std::uint32_t validFrames_ = 0;
    std::uint32_t duplicateFrames_ = 0;
    std::uint32_t overflowFrames_ = 0;

    std::uint32_t storedSamples_ = 0;

    BandSamples left_{};
    BandSamples center_{};
    BandSamples right_{};

    std::array<std::int16_t, kMaxSamples> deltas_{};
};

} // namespace ambilight
