#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/HeapBuffer.h"
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
    PerimeterGainSnapshot() {
        configureTopology(
            LedMappingProfile{});
    }

    // Generation/timestamp belong to the source ToF geometry frame.
    std::uint32_t generation = 0;
    std::uint64_t timestampUs = 0;

    std::array<
        PerimeterSegmentGain,
        static_cast<std::size_t>(SegmentId::Count)>
        segment{};

    // Exact target Q12 for every logical LED. Storage follows the active
    // topology; allocation happens only when topology changes or a slow ToF
    // snapshot is rebuilt.
    HeapBuffer<std::uint16_t> logicalGainQ12{};

    LedMappingProfile topology{};

    bool configureTopology(
        const LedMappingProfile& activeTopology) {

        if (!activeTopology.valid()) {
            return false;
        }

        HeapBuffer<std::uint16_t> candidate;

        if (!candidate.resize(
                activeTopology.totalLedCount(),
                kGainUnityQ12)) {

            return false;
        }

        logicalGainQ12.swap(candidate);
        topology = activeTopology;
        return true;
    }

    bool storageValid() const {
        return
            topology.valid() &&
            logicalGainQ12.size() >=
                topology.totalLedCount();
    }

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
