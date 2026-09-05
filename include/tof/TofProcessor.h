#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "tof/TofGrid.h"
#include "tof/TofPlaneEstimator.h"
#include "tof/TofTypes.h"

namespace ambilight {

struct TofProcessorConfig {
    TofGridTransform transform{};

    std::uint16_t minDistanceMm = 50;
    std::uint16_t maxDistanceMm = 4000;

    // Spatial robustness:
    // accepted if |x - median| <= max(minOutlierWindowMm, madMultiplier * MAD)
    std::uint16_t minOutlierWindowMm = 100;
    std::uint8_t madMultiplier = 4;

    // Minimum accepted zones for 3-column side bands and 2-column center band.
    std::uint8_t minSideZones = 6;
    std::uint8_t minCenterZones = 4;

    // Temporal filter. With 10 Hz sensor data and 600 ms time constant,
    // alpha is about 0.14 per sample.
    std::uint32_t filterTimeConstantMs = 600;
    std::uint16_t deadbandMm = 10;

    TofPlaneEstimatorConfig plane{};
};

class TofProcessor {
public:
    explicit TofProcessor(
        TofProcessorConfig config = {});

    TofGeometrySnapshot process(const TofRawFrame& raw);

    const TofGeometrySnapshot& latest() const {
        return latest_;
    }

    void reset();

    void setTransform(
        const TofGridTransform& transform);

    TofGridTransform transform() const {
        return config_.transform;
    }

    static constexpr std::size_t rawIndexForNormalized(
        std::size_t normalizedRow,
        std::size_t normalizedCol,
        const TofGridTransform& transform);

private:
    struct FilterState {
        bool initialized = false;
        std::uint16_t filteredMm = 0;
        std::uint64_t timestampUs = 0;
    };

    static bool statusUsable(std::uint8_t status);

    bool sampleUsable(
        const TofRawFrame& raw,
        std::size_t normalizedRow,
        std::size_t normalizedCol,
        std::uint16_t& distanceMm) const;

    TofBandEstimate estimateBand(
        const TofRawFrame& raw,
        std::size_t firstColumn,
        std::size_t lastColumnInclusive,
        std::uint8_t minimumAccepted,
        FilterState& filterState);

    static std::uint16_t median(
        std::array<std::uint16_t, 24>& values,
        std::size_t count);

    std::uint16_t applyTemporalFilter(
        std::uint16_t measurementMm,
        std::uint64_t timestampUs,
        FilterState& state);

    TofProcessorConfig config_{};
    TofPlaneEstimator planeEstimator_;

    FilterState leftFilter_{};
    FilterState centerFilter_{};
    FilterState rightFilter_{};

    TofGeometrySnapshot latest_{};
};

constexpr std::size_t TofProcessor::rawIndexForNormalized(
    std::size_t normalizedRow,
    std::size_t normalizedCol,
    const TofGridTransform& transform) {

    return tofRawIndexForNormalized(
        normalizedRow,
        normalizedCol,
        transform);
}

} // namespace ambilight
