#include "tof/TofPerimeterGainModel.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ambilight {
namespace {

std::uint16_t clampUnsignedMm(double value) {
    if (value <= 0.0) {
        return 0;
    }

    if (value >= 65535.0) {
        return 65535;
    }

    return static_cast<std::uint16_t>(
        std::lround(value));
}

std::uint16_t ratioPermille(
    double requestedHalfSpanMm,
    std::uint16_t observedHalfSpanMm) {

    if (observedHalfSpanMm == 0) {
        return 65535;
    }

    const double ratio =
        std::abs(requestedHalfSpanMm) /
        static_cast<double>(
            observedHalfSpanMm) *
        1000.0;

    if (ratio >= 65535.0) {
        return 65535;
    }

    return static_cast<std::uint16_t>(
        std::lround(ratio));
}

} // namespace

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

    snapshot.logicalGainQ12.fill(
        kGainUnityQ12);

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
    next.projectionUsable = true;
    next.failOpen = false;

    next.observedHalfSpanXmm =
        geometry.plane.observedHalfSpanXmm;

    next.observedHalfSpanYmm =
        geometry.plane.observedHalfSpanYmm;

    double maxAbsScreenX = 0.0;
    double maxAbsScreenY = 0.0;

    for (const auto& segmentGeometry :
         config_.geometry) {

        maxAbsScreenX =
            std::max(
                maxAbsScreenX,
                std::max(
                    std::abs(
                        static_cast<double>(
                            segmentGeometry.logicalStart.xMm)),
                    std::abs(
                        static_cast<double>(
                            segmentGeometry.logicalEnd.xMm))));

        maxAbsScreenY =
            std::max(
                maxAbsScreenY,
                std::max(
                    std::abs(
                        static_cast<double>(
                            segmentGeometry.logicalStart.yMm)),
                    std::abs(
                        static_cast<double>(
                            segmentGeometry.logicalEnd.yMm))));
    }

    next.screenHalfSpanXmm =
        clampUnsignedMm(
            maxAbsScreenX);

    next.screenHalfSpanYmm =
        clampUnsignedMm(
            maxAbsScreenY);

    next.extrapolationXPermille =
        ratioPermille(
            maxAbsScreenX,
            next.observedHalfSpanXmm);

    next.extrapolationYPermille =
        ratioPermille(
            maxAbsScreenY,
            next.observedHalfSpanYmm);

    next.extrapolationWarning =
        next.extrapolationXPermille >
            config_.maxRecommendedExtrapolationPermille ||
        next.extrapolationYPermille >
            config_.maxRecommendedExtrapolationPermille;

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
                    true);

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
                    true);

            return latest_;
        }

        const auto& logicalSegment =
            kSegments[index];

        if (logicalSegment.id !=
                segmentGeometry.id ||
            logicalSegment.logicalLength == 0) {

            latest_ =
                unitySnapshot(
                    generation,
                    nowUs,
                    true);

            return latest_;
        }

        const std::int32_t distanceSpan =
            static_cast<std::int32_t>(
                segment.endDistanceMm) -
            static_cast<std::int32_t>(
                segment.startDistanceMm);

        const std::uint32_t denominator =
            logicalSegment.logicalLength > 1
                ? logicalSegment.logicalLength - 1
                : 1;

        for (std::uint16_t offset = 0;
             offset <
                logicalSegment.logicalLength;
             ++offset) {

            const std::int64_t numerator =
                static_cast<std::int64_t>(
                    distanceSpan) *
                offset;

            const std::int64_t rounded =
                numerator >= 0
                    ? numerator +
                        static_cast<std::int64_t>(
                            denominator / 2U)
                    : numerator -
                        static_cast<std::int64_t>(
                            denominator / 2U);

            const std::int32_t distance =
                static_cast<std::int32_t>(
                    segment.startDistanceMm) +
                static_cast<std::int32_t>(
                    rounded /
                    static_cast<std::int64_t>(
                        denominator));

            if (distance <
                    config_.minDistanceMm ||
                distance >
                    config_.maxDistanceMm) {

                latest_ =
                    unitySnapshot(
                        generation,
                        nowUs,
                        true);

                return latest_;
            }

            const std::uint16_t logicalIndex =
                static_cast<std::uint16_t>(
                    logicalSegment.logicalStart +
                    offset);

            next.logicalGainQ12[
                logicalIndex] =
                config_.curve.evaluate(
                    static_cast<std::uint16_t>(
                        distance));
        }

        segment.startQ12 =
            next.logicalGainQ12[
                logicalSegment.logicalStart];

        segment.endQ12 =
            next.logicalGainQ12[
                logicalSegment.logicalStart +
                logicalSegment.logicalLength -
                1];

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
