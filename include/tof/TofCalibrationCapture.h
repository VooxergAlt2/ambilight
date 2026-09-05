#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "tof/TofPerimeterGainModel.h"
#include "tof/TofTypes.h"

namespace ambilight {

struct CalibrationUnsignedSummary {
    bool valid = false;

    std::uint16_t p10 = 0;
    std::uint16_t median = 0;
    std::uint16_t p90 = 0;
};

struct CalibrationSignedSummary {
    bool valid = false;

    std::int16_t p10 = 0;
    std::int16_t median = 0;
    std::int16_t p90 = 0;
};

struct CalibrationBandSummary {
    bool valid = false;

    std::uint16_t p10Mm = 0;
    std::uint16_t medianMm = 0;
    std::uint16_t p90Mm = 0;

    std::uint16_t medianMadMm = 0;

    std::uint8_t minAccepted = 0;
    std::uint8_t maxAccepted = 0;
};

struct CalibrationPlaneSummary {
    bool valid = false;
    std::uint32_t validFrames = 0;

    CalibrationSignedSummary yawCentiDeg{};
    CalibrationSignedSummary pitchCentiDeg{};
    CalibrationUnsignedSummary interceptMm{};

    std::uint16_t medianResidualMadMm = 0;

    std::uint8_t minAccepted = 0;
    std::uint8_t maxAccepted = 0;

    std::uint16_t medianObservedHalfSpanXmm = 0;
    std::uint16_t medianObservedHalfSpanYmm = 0;
};

struct CalibrationSegmentDistanceSummary {
    CalibrationUnsignedSummary startMm{};
    CalibrationUnsignedSummary endMm{};
};

struct CalibrationSpatialSummary {
    bool valid = false;
    std::uint32_t validFrames = 0;
    std::uint32_t warningFrames = 0;

    CalibrationUnsignedSummary minDistanceMm{};
    CalibrationUnsignedSummary maxDistanceMm{};

    std::uint16_t medianExtrapolationXPermille = 0;
    std::uint16_t medianExtrapolationYPermille = 0;

    std::array<
        CalibrationSegmentDistanceSummary,
        static_cast<std::size_t>(SegmentId::Count)>
        segment{};
};

struct CalibrationCaptureSummary {
    std::uint32_t totalFrames = 0;

    // Legacy robust-band validity count.
    std::uint32_t validFrames = 0;

    std::uint32_t duplicateFrames = 0;
    std::uint32_t overflowFrames = 0;

    CalibrationBandSummary left{};
    CalibrationBandSummary center{};
    CalibrationBandSummary right{};

    std::int16_t medianRightMinusLeftMm = 0;

    CalibrationPlaneSummary plane{};
    CalibrationSpatialSummary spatial{};
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

    // Legacy geometry-only overload retained for native tests and tools.
    bool ingest(
        const TofGeometrySnapshot& geometry);

    bool ingest(
        const TofGeometrySnapshot& geometry,
        const PerimeterGainSnapshot& spatial);

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

    struct PlaneSamples {
        std::array<std::int16_t, kMaxSamples> yawCentiDeg{};
        std::array<std::int16_t, kMaxSamples> pitchCentiDeg{};
        std::array<std::uint16_t, kMaxSamples> interceptMm{};
        std::array<std::uint16_t, kMaxSamples> residualMadMm{};
        std::array<std::uint8_t, kMaxSamples> accepted{};
        std::array<std::uint16_t, kMaxSamples> observedHalfSpanXmm{};
        std::array<std::uint16_t, kMaxSamples> observedHalfSpanYmm{};
    };

    struct SpatialSamples {
        std::array<std::uint16_t, kMaxSamples> minDistanceMm{};
        std::array<std::uint16_t, kMaxSamples> maxDistanceMm{};
        std::array<std::uint16_t, kMaxSamples> extrapolationXPermille{};
        std::array<std::uint16_t, kMaxSamples> extrapolationYPermille{};

        std::array<
            std::array<std::uint16_t, kMaxSamples>,
            static_cast<std::size_t>(SegmentId::Count)>
            startDistanceMm{};

        std::array<
            std::array<std::uint16_t, kMaxSamples>,
            static_cast<std::size_t>(SegmentId::Count)>
            endDistanceMm{};
    };

    static std::uint16_t percentile(
        std::array<std::uint16_t, kMaxSamples> values,
        std::size_t count,
        std::uint8_t percentile);

    static std::int16_t percentileSigned(
        std::array<std::int16_t, kMaxSamples> values,
        std::size_t count,
        std::uint8_t percentile);

    static CalibrationUnsignedSummary summarizeUnsigned(
        const std::array<std::uint16_t, kMaxSamples>& values,
        std::size_t count);

    static CalibrationSignedSummary summarizeSigned(
        const std::array<std::int16_t, kMaxSamples>& values,
        std::size_t count);

    static CalibrationBandSummary summarizeBand(
        const BandSamples& samples,
        std::size_t count);

    static std::int16_t summarizeDelta(
        std::array<std::int16_t, kMaxSamples> values,
        std::size_t count);

    static std::uint16_t clampUnsignedMm(float value);

    bool captureBands(
        const TofGeometrySnapshot& geometry);

    bool capturePlane(
        const TofPlaneEstimate& plane);

    bool captureSpatial(
        const PerimeterGainSnapshot& spatial);

    bool active_ = false;

    std::uint64_t startedUs_ = 0;
    std::uint64_t durationUs_ = kDefaultDurationUs;

    std::uint32_t lastGeometryGeneration_ = 0;

    std::uint32_t totalFrames_ = 0;
    std::uint32_t validFrames_ = 0;
    std::uint32_t duplicateFrames_ = 0;
    std::uint32_t overflowFrames_ = 0;

    std::uint32_t storedSamples_ = 0;
    std::uint32_t planeSamplesCount_ = 0;
    std::uint32_t spatialSamplesCount_ = 0;
    std::uint32_t spatialWarningFrames_ = 0;

    BandSamples left_{};
    BandSamples center_{};
    BandSamples right_{};

    std::array<std::int16_t, kMaxSamples> deltas_{};

    PlaneSamples plane_{};
    SpatialSamples spatial_{};
};

} // namespace ambilight
