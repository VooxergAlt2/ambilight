#include "tof/TofPlaneChangeGate.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ambilight {

float TofPlaneChangeGate::wallZ(
    const TofPlaneEstimate& plane,
    const ScreenPointMm& point) {

    return
        plane.interceptMm +
        plane.slopeX * point.xMm +
        plane.slopeY * point.yMm;
}

float TofPlaneChangeGate::maxWallDeltaMm(
    const TofPlaneEstimate& candidate) const {

    float maximum = 0.0F;

    // A difference between two planes is itself a plane. Over a rectangular
    // screen its maximum absolute Z difference occurs at a corner. Segment
    // endpoints contain those four corners, so no per-LED scan is needed
    // merely to decide whether a recalculation is worthwhile.
    for (const auto& segment : config_.geometry) {
        const ScreenPointMm points[] = {
            segment.logicalStart,
            segment.logicalEnd
        };

        for (const auto& point : points) {
            const float candidateZ =
                wallZ(candidate, point);

            const float acceptedZ =
                wallZ(acceptedPlane_, point);

            const float delta =
                std::abs(candidateZ - acceptedZ);

            if (!std::isfinite(delta)) {
                return std::numeric_limits<float>::infinity();
            }

            maximum =
                std::max(
                    maximum,
                    delta);
        }
    }

    return maximum;
}

TofPlaneChangeDecision TofPlaneChangeGate::observe(
    const TofPlaneEstimate& candidate) {

    TofPlaneChangeDecision decision;

    if (!candidate.valid) {
        if (haveAcceptedPlane_) {
            haveAcceptedPlane_ = false;
            acceptedPlane_ = {};
            decision.action =
                TofPlaneChangeAction::FailOpen;
        }

        return decision;
    }

    if (!haveAcceptedPlane_) {
        acceptedPlane_ = candidate;
        haveAcceptedPlane_ = true;
        decision.action =
            TofPlaneChangeAction::Recalculate;
        return decision;
    }

    decision.maxWallDeltaMm =
        maxWallDeltaMm(candidate);

    const float threshold =
        std::max(
            0.0F,
            config_.wallDeltaDeadbandMm);

    if (!std::isfinite(
            decision.maxWallDeltaMm) ||
        decision.maxWallDeltaMm >= threshold) {

        acceptedPlane_ = candidate;
        decision.action =
            TofPlaneChangeAction::Recalculate;
        return decision;
    }

    // Do not move the accepted reference here. This makes the deadband
    // cumulative: several small real movements eventually exceed the
    // threshold relative to the last applied plane.
    decision.action =
        TofPlaneChangeAction::RefreshOnly;

    return decision;
}

void TofPlaneChangeGate::reset() {
    haveAcceptedPlane_ = false;
    acceptedPlane_ = {};
}

} // namespace ambilight
