#pragma once

#include <cstdint>

#include "core/ScreenGeometry.h"
#include "tof/TofTypes.h"

namespace ambilight {

enum class TofPlaneChangeAction : std::uint8_t {
    None = 0,
    RefreshOnly,
    Recalculate,
    FailOpen
};

struct TofPlaneChangeDecision {
    TofPlaneChangeAction action = TofPlaneChangeAction::None;

    // Maximum absolute wall-Z difference between the candidate plane and the
    // last accepted plane over the configured LED rectangle.
    float maxWallDeltaMm = 0.0F;
};

struct TofPlaneChangeGateConfig {
    PerimeterScreenGeometry geometry{};

    // A new valid plane is accepted only when it moves the predicted wall
    // position by at least this much somewhere on the LED rectangle.
    float wallDeltaDeadbandMm = 10.0F;
};

class TofPlaneChangeGate {
public:
    explicit TofPlaneChangeGate(
        TofPlaneChangeGateConfig config = {})
        : config_(config) {}

    TofPlaneChangeDecision observe(
        const TofPlaneEstimate& candidate);

    void reset();

    bool hasAcceptedPlane() const {
        return haveAcceptedPlane_;
    }

    const TofPlaneEstimate& acceptedPlane() const {
        return acceptedPlane_;
    }

private:
    static float wallZ(
        const TofPlaneEstimate& plane,
        const ScreenPointMm& point);

    float maxWallDeltaMm(
        const TofPlaneEstimate& candidate) const;

    TofPlaneChangeGateConfig config_{};

    bool haveAcceptedPlane_ = false;
    TofPlaneEstimate acceptedPlane_{};
};

} // namespace ambilight
