#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/ScreenGeometry.h"
#include "led/LedMappingProfile.h"
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
    // Generation/timestamp belong to the source ToF geometry frame.
    std::uint32_t generation = 0;
    std::uint64_t timestampUs = 0;

    std::array<
        PerimeterSegmentGain,
        static_cast<std::size_t>(SegmentId::Count)>
        segment{};

    // Exact target Q12 for every logical LED. Each LED position is projected
    // along screen +Z onto the fitted wall plane and evaluated independently.
    std::array<
        std::uint16_t,
        config::kLogicalLedCapacity>
        logicalGainQ12{};

    LedMappingProfile topology{};

    std::uint16_t minDistanceMm = 0;
    std::uint16_t maxDistanceMm = 0;

    bool planeUsable = false;
    bool projectionUsable = false;
    bool failOpen = true;
};

struct TofPerimeterGainModelConfig {
    DistanceGainCurve curve{};
    PerimeterScreenGeometry geometry{};
    LedMappingProfile topology{};

    std::uint16_t minDistanceMm = 30;
    std::uint16_t maxDistanceMm = 4000;

    // Normal runtime samples the TV pose roughly every 12 seconds.
    std::uint64_t staleTimeoutUs = 30000000;
};

class TofPerimeterGainModel {
public:
    explicit TofPerimeterGainModel(
        TofPerimeterGainModelConfig config = {})
        : config_(config) {}

    bool setGeometry(
        const PerimeterScreenGeometry& geometry) {

        config_.geometry = geometry;
        return true;
    }

    bool setTopology(
        const LedMappingProfile& topology) {

        if (!topology.valid()) {
            return false;
        }

        config_.topology = topology;
        return true;
    }

    bool setCurve(
        const DistanceGainCurve& curve) {

        if (!curve.valid()) {
            return false;
        }

        config_.curve = curve;
        return true;
    }

    PerimeterGainSnapshot evaluate(
        const TofGeometrySnapshot& geometry,
        std::uint64_t nowUs);

    const PerimeterGainSnapshot& latest() const {
        return latest_;
    }

private:
    PerimeterGainSnapshot unitySnapshot(
        std::uint32_t generation,
        std::uint64_t sourceTimestampUs,
        bool planeUsable);

    bool distanceAt(
        const TofPlaneEstimate& plane,
        const ScreenPointMm& ledPoint,
        std::uint16_t& distanceMm) const;

    static ScreenPointMm interpolatePoint(
        const ScreenPointMm& start,
        const ScreenPointMm& end,
        std::uint16_t offset,
        std::uint16_t length);

    TofPerimeterGainModelConfig config_{};
    PerimeterGainSnapshot latest_{};
};

} // namespace ambilight
