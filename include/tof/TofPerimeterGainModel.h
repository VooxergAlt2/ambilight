#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/ScreenGeometry.h"
#include "tof/TofGainModel.h"
#include "tof/TofTypes.h"

namespace ambilight {

struct PerimeterSegmentGain {
    std::uint16_t startDistanceMm = 0;
    std::uint16_t endDistanceMm = 0;

    std::uint16_t startQ12 = kGainUnityQ12;
    std::uint16_t endQ12 = kGainUnityQ12;
};

struct PerimeterGainSnapshot {
    std::uint32_t generation = 0;
    std::uint64_t timestampUs = 0;

    std::array<
        PerimeterSegmentGain,
        static_cast<std::size_t>(SegmentId::Count)>
        segment{};

    std::uint16_t minDistanceMm = 0;
    std::uint16_t maxDistanceMm = 0;

    bool planeUsable = false;
    bool failOpen = true;
};

struct TofPerimeterGainModelConfig {
    DistanceGainCurve curve{};
    PerimeterScreenGeometry geometry{};

    std::uint16_t minDistanceMm = 30;
    std::uint16_t maxDistanceMm = 4000;

    std::uint64_t staleTimeoutUs = 1500000;
};

class TofPerimeterGainModel {
public:
    explicit TofPerimeterGainModel(
        TofPerimeterGainModelConfig config = {})
        : config_(config) {}

    PerimeterGainSnapshot evaluate(
        const TofGeometrySnapshot& geometry,
        std::uint64_t nowUs);

    const PerimeterGainSnapshot& latest() const {
        return latest_;
    }

private:
    static PerimeterGainSnapshot unitySnapshot(
        std::uint32_t generation,
        std::uint64_t nowUs,
        bool planeUsable);

    bool distanceAt(
        const TofPlaneEstimate& plane,
        const ScreenPointMm& point,
        std::uint16_t& distanceMm) const;

    TofPerimeterGainModelConfig config_{};
    PerimeterGainSnapshot latest_{};
};

} // namespace ambilight
