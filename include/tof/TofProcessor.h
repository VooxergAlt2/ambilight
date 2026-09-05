#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

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

    FilterState leftFilter_{};
    FilterState centerFilter_{};
    FilterState rightFilter_{};

    TofGeometrySnapshot latest_{};
};

constexpr std::size_t TofProcessor::rawIndexForNormalized(
    std::size_t normalizedRow,
    std::size_t normalizedCol,
    const TofGridTransform& transform) {

    std::size_t x = normalizedCol;
    std::size_t y = normalizedRow;

    if (transform.mirrorX) {
        x = kTofGridWidth - 1 - x;
    }

    std::size_t rawX = x;
    std::size_t rawY = y;

    switch (transform.rotation) {
    case TofRotation::Deg0:
        rawX = x;
        rawY = y;
        break;

    case TofRotation::Deg90:
        rawX = y;
        rawY = kTofGridHeight - 1 - x;
        break;

    case TofRotation::Deg180:
        rawX = kTofGridWidth - 1 - x;
        rawY = kTofGridHeight - 1 - y;
        break;

    case TofRotation::Deg270:
        rawX = kTofGridWidth - 1 - y;
        rawY = x;
        break;
    }

    return rawY * kTofGridWidth + rawX;
}

} // namespace ambilight
