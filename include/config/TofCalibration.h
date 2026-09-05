#pragma once

#include <array>
#include <cstddef>

#include "tof/TofGainModel.h"

namespace ambilight::config {

// Stage 9 deliberately ships a pass-through curve.
//
// Real attenuation points must be populated only after physical calibration
// captures have been reviewed. Keeping both endpoints at unity makes accidental
// use of the model harmless.
constexpr std::array<GainPoint, DistanceGainCurve::kMaxPoints>
    kTofGainPoints = {{
        {50, kGainUnityQ12},
        {4000, kGainUnityQ12},
        {},
        {},
        {},
        {},
        {},
        {}
    }};

constexpr std::size_t kTofGainPointCount = 2;

} // namespace ambilight::config
