#pragma once

#include <cstdint>

#include "core/RgbFrame.h"
#include "led/LedMappingProfile.h"

namespace ambilight {

enum class ManualLightingEffect : std::uint8_t {
    Ambilight = 0,
    Solid = 1,
    Rainbow = 2,
    Breathing = 3,
    WarmWhite = 4,
    BiasWhite = 5,
    Sunset = 6,
    Candle = 7,
    Aurora = 8,
    Twinkle = 9,
    Count
};

struct ManualLightingState {
    // Ambilight means automatic source selection: fresh DDP owns the output,
    // otherwise fallbackEffect is rendered locally. Any other value forces a
    // local effect and also becomes the remembered fallback when selected.
    ManualLightingEffect effect =
        ManualLightingEffect::Ambilight;

    ManualLightingEffect fallbackEffect =
        ManualLightingEffect::BiasWhite;

    Rgb8 color{
        255,
        255,
        255
    };

    std::uint8_t speed = 128;
    std::uint8_t intensity = 128;
};

class ManualLighting {
public:
    static constexpr std::uint8_t kEffectCount =
        static_cast<std::uint8_t>(
            ManualLightingEffect::Count);

    static bool validEffect(
        std::uint8_t effect);

    static bool validLocalEffect(
        ManualLightingEffect effect);

    static const char* effectName(
        ManualLightingEffect effect);

    static bool animated(
        ManualLightingEffect effect);

    static bool render(
        const ManualLightingState& state,
        const LedMappingProfile& topology,
        std::uint64_t nowUs,
        RgbFrame& output);
};

} // namespace ambilight
