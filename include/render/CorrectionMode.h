#pragma once

#include <cstdint>

#include "core/RgbFrame.h"
#include "render/RenderGainContext.h"

namespace ambilight {

enum class CorrectionMode : std::uint8_t {
    Disabled = 0,
    Shadow = 1,
    Active = 2
};

constexpr bool correctionModeValid(
    std::uint8_t raw) {

    return raw <=
        static_cast<std::uint8_t>(
            CorrectionMode::Active);
}

constexpr const char* correctionModeName(
    CorrectionMode mode) {

    switch (mode) {
    case CorrectionMode::Disabled:
        return "DISABLED";
    case CorrectionMode::Shadow:
        return "SHADOW";
    case CorrectionMode::Active:
        return "ACTIVE";
    }

    return "INVALID";
}

class CorrectionOutputPolicy {
public:
    static constexpr bool evaluatesGain(
        CorrectionMode mode) {

        return mode !=
            CorrectionMode::Disabled;
    }

    static constexpr bool physicallyAppliesGain(
        CorrectionMode mode) {

        return mode ==
            CorrectionMode::Active;
    }

    static constexpr Rgb8 physicalOutput(
        CorrectionMode mode,
        const Rgb8& original,
        const ShadowPixelResult& preview) {

        return physicallyAppliesGain(mode)
            ? preview.wouldOutput
            : original;
    }
};

} // namespace ambilight
