#include "tof/TofPerimeterGainModel.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ambilight {

PerimeterGainSnapshot TofPerimeterGainModel::unitySnapshot(
    std::uint32_t generation,
    std::uint64_t nowUs,
    bool planeUsable) {

    PerimeterGainSnapshot snapshot;
    snapshot.generation = generation;
    snapshot.timestampUs = nowUs;
    snapshot.planeUsable = planeUsable;
    snapshot.failOpen = true;

    for (auto& segment : snapshot.segment) {
        segment.startQ12 = kGainUnityQ12;
        segment.endQ12 = kGainUnityQ12;
    }

    return snapshot;
}

bool TofPerimeterGainModel::distanceAt(
    const TofPlaneEstimate& plane,
    const ScreenPointMm& point,
    std::uint16_t& distanceMm) const {

    if (!plane.valid) {
        return false;
    }

    const double wallZ =
        static_cast<double>(plane.interceptMm) +
        static_cast<double>(plane.slopeX) *
            static_cast<double>(point.xMm) +
        static_cast<double>(plane.slopeY) *
            static_cast<double>(point.yMm);

    // Point Z is the LED emitter plane relative to the ToF optical origin.
    // We intentionally use separation along TV/sensor +Z, not the shortest
    // orthogonal distance to the wall plane.
    const double gapMm =
        wallZ -
        static_cast<double>(point.zMm);

    if (!std::isfinite(gapMm) ||
        gapMm <
            static_cast<double>(
                config_.minDistanceMm) ||
        gapMm >
            static_cast<double>(
                config_.maxDistanceMm)) {
        return false;
    }

    distanceMm =
        static_cast<std::uint16_t>(
            std::lround(gapMm));

    return true;
}

PerimeterGainSnapshot TofPerimeterGainModel::evaluate(
    const TofGeometrySnapshot& geometry,
    std::uint64_t nowUs) {

    const std::uint32_t generation =
        latest_.generation + 1;

    const bool timestampUsable =
        geometry.timestampUs != 0 &&
        nowUs >= geometry.timestampUs &&
        nowUs - geometry.timestampUs <=
            config_.staleTimeoutUs;

    const bool planeUsable =
        geometry.plane.valid &&
        timestampUsable;

    if (!planeUsable ||
        !config_.curve.valid()) {

        latest_ =
            unitySnapshot(
                generation,
                nowUs,
                planeUsable);

        return latest_;
    }

    PerimeterGainSnapshot next;
    next.generation = generation;
    next.timestampUs = nowUs;
    next.planeUsable = true;
    next.failOpen = false;

    std::uint16_t minDistance =
        std::numeric_limits<std::uint16_t>::max();

    std::uint16_t maxDistance = 0;

    for (const auto& segmentGeometry :
         config_.geometry) {

        const auto index =
            static_cast<std::size_t>(
                segmentGeometry.id);

        if (index >= next.segment.size()) {
            latest_ =
                unitySnapshot(
                    generation,
                    nowUs,
                    false);

            return latest_;
        }

        auto& segment =
            next.segment[index];

        if (!distanceAt(
                geometry.plane,
                segmentGeometry.logicalStart,
                segment.startDistanceMm) ||
            !distanceAt(
                geometry.plane,
                segmentGeometry.logicalEnd,
                segment.endDistanceMm)) {

            latest_ =
                unitySnapshot(
                    generation,
                    nowUs,
                    false);

            return latest_;
        }

        segment.startQ12 =
            config_.curve.evaluate(
                segment.startDistanceMm);

        segment.endQ12 =
            config_.curve.evaluate(
                segment.endDistanceMm);

        minDistance =
            std::min(
                minDistance,
                std::min(
                    segment.startDistanceMm,
                    segment.endDistanceMm));

        maxDistance =
            std::max(
                maxDistance,
                std::max(
                    segment.startDistanceMm,
                    segment.endDistanceMm));
    }

    next.minDistanceMm =
        minDistance;

    next.maxDistanceMm =
        maxDistance;

    latest_ = next;
    return latest_;
}

} // namespace ambilight
