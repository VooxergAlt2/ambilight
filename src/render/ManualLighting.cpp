#include "render/ManualLighting.h"

#include <cstddef>
#include <cstdint>

namespace ambilight {
namespace {

Rgb8 scaleColor(
    const Rgb8& color,
    std::uint8_t level) {

    const auto scale =
        [level](
            std::uint8_t value) {

            return
                static_cast<std::uint8_t>(
                    (
                        static_cast<
                            std::uint16_t>(
                                value) *
                        level +
                        127U
                    ) /
                    255U);
        };

    return Rgb8{
        scale(color.r),
        scale(color.g),
        scale(color.b)
    };
}

Rgb8 wheel(
    std::uint8_t position) {

    if (position < 85U) {
        return Rgb8{
            static_cast<std::uint8_t>(
                255U -
                position * 3U),
            static_cast<std::uint8_t>(
                position * 3U),
            0
        };
    }

    if (position < 170U) {
        position =
            static_cast<std::uint8_t>(
                position -
                85U);

        return Rgb8{
            0,
            static_cast<std::uint8_t>(
                255U -
                position * 3U),
            static_cast<std::uint8_t>(
                position * 3U)
        };
    }

    position =
        static_cast<std::uint8_t>(
            position -
            170U);

    return Rgb8{
        static_cast<std::uint8_t>(
            position * 3U),
        0,
        static_cast<std::uint8_t>(
            255U -
            position * 3U)
    };
}

std::uint64_t animationPeriodUs(
    std::uint8_t speed,
    std::uint64_t slowUs,
    std::uint64_t fastUs) {

    const std::uint64_t span =
        slowUs -
        fastUs;

    return
        slowUs -
        (
            span *
            speed
        ) /
        255ULL;
}

std::uint8_t phaseFor(
    std::uint64_t nowUs,
    std::uint64_t periodUs) {

    if (periodUs == 0) {
        return 0;
    }

    return
        static_cast<std::uint8_t>(
            (
                (nowUs % periodUs) *
                256ULL
            ) /
            periodUs);
}

std::uint8_t triangle(
    std::uint8_t phase) {

    if (phase < 128U) {
        return
            static_cast<std::uint8_t>(
                phase * 2U);
    }

    return
        static_cast<std::uint8_t>(
            (
                255U -
                phase
            ) *
            2U);
}

} // namespace

bool ManualLighting::validEffect(
    std::uint8_t effect) {

    return
        effect <
        kEffectCount;
}

const char* ManualLighting::effectName(
    ManualLightingEffect effect) {

    switch (effect) {
    case ManualLightingEffect::Ambilight:
        return "Ambilight";

    case ManualLightingEffect::Solid:
        return "Solid";

    case ManualLightingEffect::Rainbow:
        return "Rainbow";

    case ManualLightingEffect::Breathing:
        return "Breathing";

    case ManualLightingEffect::Count:
        break;
    }

    return "Unknown";
}

bool ManualLighting::animated(
    ManualLightingEffect effect) {

    return
        effect ==
            ManualLightingEffect::Rainbow ||
        effect ==
            ManualLightingEffect::Breathing;
}

bool ManualLighting::render(
    const ManualLightingState& state,
    const LedMappingProfile& topology,
    std::uint64_t nowUs,
    RgbFrame& output) {

    if (!topology.valid() ||
        state.effect ==
            ManualLightingEffect::Ambilight ||
        state.effect ==
            ManualLightingEffect::Count) {

        return false;
    }

    const std::uint16_t count =
        topology.totalLedCount();

    if (count == 0 ||
        count >
            output.pixels.size()) {

        return false;
    }

    output.clear();
    output.pixelCount =
        count;

    output.receivedUs =
        nowUs;

    switch (state.effect) {
    case ManualLightingEffect::Solid:
        for (std::size_t index = 0;
             index < count;
             ++index) {

            output.pixels[index] =
                state.color;
        }
        break;

    case ManualLightingEffect::Rainbow: {
        const std::uint64_t periodUs =
            animationPeriodUs(
                state.speed,
                12000000ULL,
                1200000ULL);

        const std::uint8_t basePhase =
            phaseFor(
                nowUs,
                periodUs);

        for (std::size_t index = 0;
             index < count;
             ++index) {

            const std::uint8_t spatial =
                static_cast<std::uint8_t>(
                    (
                        index *
                        256ULL
                    ) /
                    count);

            output.pixels[index] =
                wheel(
                    static_cast<
                        std::uint8_t>(
                            basePhase +
                            spatial));
        }
        break;
    }

    case ManualLightingEffect::Breathing: {
        const std::uint64_t periodUs =
            animationPeriodUs(
                state.speed,
                8000000ULL,
                1400000ULL);

        const std::uint8_t pulse =
            triangle(
                phaseFor(
                    nowUs,
                    periodUs));

        const std::uint8_t minimum =
            static_cast<std::uint8_t>(
                255U -
                state.intensity);

        const std::uint8_t level =
            static_cast<std::uint8_t>(
                minimum +
                (
                    static_cast<
                        std::uint16_t>(
                            pulse) *
                    state.intensity
                ) /
                    255U);

        const Rgb8 color =
            scaleColor(
                state.color,
                level);

        for (std::size_t index = 0;
             index < count;
             ++index) {

            output.pixels[index] =
                color;
        }
        break;
    }

    case ManualLightingEffect::Ambilight:
    case ManualLightingEffect::Count:
        return false;
    }

    return true;
}

} // namespace ambilight
