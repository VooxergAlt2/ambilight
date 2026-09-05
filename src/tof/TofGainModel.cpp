#include "tof/TofGainModel.h"

#include <algorithm>

namespace ambilight {

DistanceGainCurve::DistanceGainCurve() {
    std::array<GainPoint, kMaxPoints> identity{};
    identity[0] = {50, kGainUnityQ12};
    identity[1] = {4000, kGainUnityQ12};

    configure(identity, 2);
}

DistanceGainCurve::DistanceGainCurve(
    const std::array<GainPoint, kMaxPoints>& points,
    std::size_t count) {

    configure(points, count);
}

bool DistanceGainCurve::configure(
    const std::array<GainPoint, kMaxPoints>& points,
    std::size_t count) {

    if (count < 2 || count > kMaxPoints) {
        valid_ = false;
        count_ = 0;
        return false;
    }

    for (std::size_t index = 0; index < count; ++index) {
        if (points[index].gainQ12 > kGainUnityQ12) {
            valid_ = false;
            count_ = 0;
            return false;
        }

        if (index == 0) {
            continue;
        }

        if (points[index].distanceMm <=
            points[index - 1].distanceMm) {
            valid_ = false;
            count_ = 0;
            return false;
        }

        // Our model only attenuates as the TV moves closer to the wall.
        // Therefore gain must be non-decreasing as distance increases.
        if (points[index].gainQ12 <
            points[index - 1].gainQ12) {
            valid_ = false;
            count_ = 0;
            return false;
        }
    }

    points_ = points;
    count_ = count;
    valid_ = true;

    return true;
}

std::uint16_t DistanceGainCurve::evaluate(
    std::uint16_t distanceMm) const {

    if (!valid_ || count_ < 2) {
        return kGainUnityQ12;
    }

    if (distanceMm <= points_[0].distanceMm) {
        return points_[0].gainQ12;
    }

    if (distanceMm >= points_[count_ - 1].distanceMm) {
        return points_[count_ - 1].gainQ12;
    }

    for (std::size_t index = 1; index < count_; ++index) {
        const auto& upper = points_[index];

        if (distanceMm > upper.distanceMm) {
            continue;
        }

        const auto& lower = points_[index - 1];

        const std::uint32_t distanceSpan =
            static_cast<std::uint32_t>(
                upper.distanceMm - lower.distanceMm);

        const std::uint32_t distanceOffset =
            static_cast<std::uint32_t>(
                distanceMm - lower.distanceMm);

        const std::int32_t gainSpan =
            static_cast<std::int32_t>(upper.gainQ12) -
            static_cast<std::int32_t>(lower.gainQ12);

        const std::int32_t interpolated =
            static_cast<std::int32_t>(lower.gainQ12) +
            static_cast<std::int32_t>(
                (static_cast<std::int64_t>(gainSpan) *
                 distanceOffset +
                 static_cast<std::int64_t>(distanceSpan / 2U)) /
                static_cast<std::int64_t>(distanceSpan));

        return static_cast<std::uint16_t>(
            std::max<std::int32_t>(
                0,
                std::min<std::int32_t>(
                    kGainUnityQ12,
                    interpolated)));
    }

    return kGainUnityQ12;
}

TofGainModel::TofGainModel(
    TofGainModelConfig config)
    : config_(config) {}

bool TofGainModel::setCurve(
    const DistanceGainCurve& curve) {

    if (!curve.valid()) {
        return false;
    }

    config_.curve = curve;
    return true;
}

GainSnapshot TofGainModel::unitySnapshot(
    std::uint32_t generation,
    std::uint64_t sourceTimestampUs,
    bool geometryUsable) {

    GainSnapshot snapshot;
    snapshot.generation = generation;
    snapshot.timestampUs = sourceTimestampUs;

    snapshot.topQ12 = kGainUnityQ12;
    snapshot.bottomQ12 = kGainUnityQ12;
    snapshot.leftQ12 = kGainUnityQ12;
    snapshot.rightQ12 = kGainUnityQ12;

    snapshot.geometryUsable = geometryUsable;
    snapshot.failOpen = true;

    return snapshot;
}

GainSnapshot TofGainModel::evaluate(
    const TofGeometrySnapshot& geometry,
    std::uint64_t nowUs) {

    const std::uint32_t generation =
        geometry.generation;

    const bool timestampUsable =
        geometry.timestampUs != 0 &&
        nowUs >= geometry.timestampUs &&
        nowUs - geometry.timestampUs <=
            config_.staleTimeoutUs;

    const bool geometryUsable =
        geometry.valid &&
        geometry.left.valid &&
        geometry.center.valid &&
        geometry.right.valid &&
        timestampUsable;

    if (!geometryUsable || !config_.curve.valid()) {
        latest_ = unitySnapshot(
            generation,
            geometry.timestampUs,
            geometryUsable);

        return latest_;
    }

    GainSnapshot next;
    next.generation = generation;
    next.timestampUs = geometry.timestampUs;

    next.leftQ12 =
        config_.curve.evaluate(
            geometry.left.filteredMm);

    next.rightQ12 =
        config_.curve.evaluate(
            geometry.right.filteredMm);

    const std::uint16_t centerGain =
        config_.curve.evaluate(
            geometry.center.filteredMm);

    // V1 assumption only: top/bottom use the center-band wall distance.
    // This remains a model output, not an active renderer input.
    next.topQ12 = centerGain;
    next.bottomQ12 = centerGain;

    next.geometryUsable = true;
    next.failOpen = false;

    latest_ = next;
    return latest_;
}

} // namespace ambilight
