#include "tof/TofPerimeterGainModel.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ambilight {

PerimeterGainSnapshot TofPerimeterGainModel::unitySnapshot(
    std::uint32_t generation,
    std::uint64_t sourceTimestampUs,
    bool planeUsable) {

    PerimeterGainSnapshot snapshot;
    snapshot.generation = generation;
    snapshot.timestampUs = sourceTimestampUs;
    snapshot.planeUsable = planeUsable;
    snapshot.failOpen = true;

    if (!snapshot.configureTopology(
            config_.topology)) {

        snapshot.logicalGainQ12.clearStorage();
        snapshot.topology =
            config_.topology;
        return snapshot;
    }

    for (auto& segment : snapshot.segment) {
        segment.startQ12 = kGainUnityQ12;
        segment.endQ12 = kGainUnityQ12;
    }

    snapshot.logicalGainQ12.fill(
        kGainUnityQ12);

    return snapshot;
}

ScreenPointMm TofPerimeterGainModel::interpolatePoint(
    const ScreenPointMm& start,
    const ScreenPointMm& end,
    std::uint16_t offset,
    std::uint16_t length) {

    if (length <= 1) {
        return start;
    }

    const float t =
        static_cast<float>(offset) /
        static_cast<float>(length - 1);

    return ScreenPointMm{
        start.xMm + (end.xMm - start.xMm) * t,
        start.yMm + (end.yMm - start.yMm) * t,
        start.zMm + (end.zMm - start.zMm) * t
    };
}

bool TofPerimeterGainModel::distanceAt(
    const TofPlaneEstimate& plane,
    const ScreenPointMm& ledPoint,
    std::uint16_t& distanceMm) const {

    if (!plane.valid) {
        return false;
    }

    // Fitted wall:
    //   z_wall = intercept + slopeX*x + slopeY*y
    //
    // The light spot for one LED is defined by the intersection of a ray
    // starting at that LED and travelling along screen/sensor +Z with this
    // plane. Therefore x/y stay equal to the LED coordinates and the required
    // throw distance is simply z_wall - z_led.
    const double wallZ =
        static_cast<double>(plane.interceptMm) +
        static_cast<double>(plane.slopeX) *
            static_cast<double>(ledPoint.xMm) +
        static_cast<double>(plane.slopeY) *
            static_cast<double>(ledPoint.yMm);

    const double gapMm =
        wallZ -
        static_cast<double>(ledPoint.zMm);

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
        geometry.generation;

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
                geometry.timestampUs,
                planeUsable);

        return latest_;
    }

    PerimeterGainSnapshot next;

    if (!next.configureTopology(
            config_.topology)) {

        latest_ =
            unitySnapshot(
                generation,
                geometry.timestampUs,
                true);

        return latest_;
    }

    next.generation = generation;
    next.timestampUs = geometry.timestampUs;
    next.planeUsable = true;
    next.projectionUsable = true;
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
                    geometry.timestampUs,
                    true);

            return latest_;
        }

        const auto logicalSegment =
            config_.topology.segmentConfig(
                segmentGeometry.id);

        if (logicalSegment.id !=
                segmentGeometry.id ||
            logicalSegment.logicalLength == 0) {

            latest_ =
                unitySnapshot(
                    generation,
                    geometry.timestampUs,
                    true);

            return latest_;
        }

        auto& segment =
            next.segment[index];

        for (std::uint16_t offset = 0;
             offset < logicalSegment.logicalLength;
             ++offset) {

            const ScreenPointMm ledPoint =
                interpolatePoint(
                    segmentGeometry.logicalStart,
                    segmentGeometry.logicalEnd,
                    offset,
                    logicalSegment.logicalLength);

            std::uint16_t distanceMm = 0;

            if (!distanceAt(
                    geometry.plane,
                    ledPoint,
                    distanceMm)) {

                latest_ =
                    unitySnapshot(
                        generation,
                        geometry.timestampUs,
                        true);

                return latest_;
            }

            const std::size_t logicalIndex =
                logicalSegment.logicalStart +
                offset;

            next.logicalGainQ12[
                logicalIndex] =
                config_.curve.evaluate(
                    distanceMm);

            minDistance =
                std::min(
                    minDistance,
                    distanceMm);

            maxDistance =
                std::max(
                    maxDistance,
                    distanceMm);

            if (offset == 0) {
                segment.startDistanceMm =
                    distanceMm;
            }

            if (offset + 1 ==
                logicalSegment.logicalLength) {

                segment.endDistanceMm =
                    distanceMm;
            }
        }

        segment.startQ12 =
            next.logicalGainQ12[
                logicalSegment.logicalStart];

        segment.endQ12 =
            next.logicalGainQ12[
                logicalSegment.logicalStart +
                logicalSegment.logicalLength -
                1];
    }

    next.minDistanceMm =
        minDistance;

    next.maxDistanceMm =
        maxDistance;

    latest_ = next;
    return latest_;
}

} // namespace ambilight
