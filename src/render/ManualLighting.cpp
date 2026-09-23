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
                        static_cast<std::uint16_t>(value) *
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

Rgb8 blend(
    const Rgb8& a,
    const Rgb8& b,
    std::uint8_t amount) {

    const auto channel =
        [amount](
            std::uint8_t from,
            std::uint8_t to) {

            const std::uint16_t inverse =
                static_cast<std::uint16_t>(
                    255U - amount);

            return
                static_cast<std::uint8_t>(
                    (
                        static_cast<std::uint16_t>(from) *
                            inverse +
                        static_cast<std::uint16_t>(to) *
                            amount +
                        127U
                    ) /
                    255U);
        };

    return Rgb8{
        channel(a.r, b.r),
        channel(a.g, b.g),
        channel(a.b, b.b)
    };
}

Rgb8 wheel(
    std::uint8_t position) {

    if (position < 85U) {
        return Rgb8{
            static_cast<std::uint8_t>(
                255U - position * 3U),
            static_cast<std::uint8_t>(
                position * 3U),
            0
        };
    }

    if (position < 170U) {
        position =
            static_cast<std::uint8_t>(
                position - 85U);

        return Rgb8{
            0,
            static_cast<std::uint8_t>(
                255U - position * 3U),
            static_cast<std::uint8_t>(
                position * 3U)
        };
    }

    position =
        static_cast<std::uint8_t>(
            position - 170U);

    return Rgb8{
        static_cast<std::uint8_t>(
            position * 3U),
        0,
        static_cast<std::uint8_t>(
            255U - position * 3U)
    };
}

std::uint64_t animationPeriodUs(
    std::uint8_t speed,
    std::uint64_t slowUs,
    std::uint64_t fastUs) {

    const std::uint64_t span =
        slowUs - fastUs;

    return
        slowUs -
        (span * speed) /
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
            (255U - phase) * 2U);
}

std::uint32_t mix32(
    std::uint32_t value) {

    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    value ^= value >> 16U;
    return value;
}

Rgb8 auroraColor(
    std::uint8_t phase) {

    constexpr Rgb8 kBlue{16, 70, 255};
    constexpr Rgb8 kCyan{0, 220, 200};
    constexpr Rgb8 kGreen{30, 255, 90};
    constexpr Rgb8 kViolet{140, 30, 255};

    if (phase < 64U) {
        return blend(
            kBlue,
            kCyan,
            static_cast<std::uint8_t>(
                phase * 4U));
    }

    if (phase < 128U) {
        return blend(
            kCyan,
            kGreen,
            static_cast<std::uint8_t>(
                (phase - 64U) * 4U));
    }

    if (phase < 192U) {
        return blend(
            kGreen,
            kViolet,
            static_cast<std::uint8_t>(
                (phase - 128U) * 4U));
    }

    return blend(
        kViolet,
        kBlue,
        static_cast<std::uint8_t>(
            (phase - 192U) * 4U));
}

} // namespace

bool ManualLighting::validEffect(
    std::uint8_t effect) {

    return
        effect <
        kEffectCount;
}

bool ManualLighting::validLocalEffect(
    ManualLightingEffect effect) {

    return
        effect >
            ManualLightingEffect::Ambilight &&
        effect <
            ManualLightingEffect::Count;
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
    case ManualLightingEffect::WarmWhite:
        return "Warm White";
    case ManualLightingEffect::BiasWhite:
        return "Bias White";
    case ManualLightingEffect::Sunset:
        return "Sunset";
    case ManualLightingEffect::Candle:
        return "Candle";
    case ManualLightingEffect::Aurora:
        return "Aurora";
    case ManualLightingEffect::Twinkle:
        return "Twinkle";
    case ManualLightingEffect::Count:
        break;
    }

    return "Unknown";
}

bool ManualLighting::animated(
    ManualLightingEffect effect) {

    switch (effect) {
    case ManualLightingEffect::Rainbow:
    case ManualLightingEffect::Breathing:
    case ManualLightingEffect::Candle:
    case ManualLightingEffect::Aurora:
    case ManualLightingEffect::Twinkle:
        return true;
    case ManualLightingEffect::Ambilight:
    case ManualLightingEffect::Solid:
    case ManualLightingEffect::WarmWhite:
    case ManualLightingEffect::BiasWhite:
    case ManualLightingEffect::Sunset:
    case ManualLightingEffect::Count:
        return false;
    }

    return false;
}

bool ManualLighting::render(
    const ManualLightingState& state,
    const LedMappingProfile& topology,
    std::uint64_t nowUs,
    RgbFrame& output) {

    if (!topology.valid() ||
        !validLocalEffect(
            state.effect)) {

        return false;
    }

    const std::size_t count =
        topology.totalLedCount();

    if (count == 0 ||
        count > output.pixels.size()) {

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
                    (index * 256ULL) /
                    count);

            output.pixels[index] =
                wheel(
                    static_cast<std::uint8_t>(
                        basePhase + spatial));
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
                255U - state.intensity);

        const std::uint8_t level =
            static_cast<std::uint8_t>(
                minimum +
                (
                    static_cast<std::uint16_t>(pulse) *
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

    case ManualLightingEffect::WarmWhite: {
        constexpr Rgb8 kWarmWhite{255, 172, 92};

        for (std::size_t index = 0;
             index < count;
             ++index) {

            output.pixels[index] =
                kWarmWhite;
        }
        break;
    }

    case ManualLightingEffect::BiasWhite: {
        // Near-D65 RGB intended as a neutral bias-light preset rather than a
        // color-temperature calibration promise for arbitrary LED strips.
        constexpr Rgb8 kBiasWhite{255, 249, 253};

        for (std::size_t index = 0;
             index < count;
             ++index) {

            output.pixels[index] =
                kBiasWhite;
        }
        break;
    }

    case ManualLightingEffect::Sunset: {
        constexpr Rgb8 kOrange{255, 78, 18};
        constexpr Rgb8 kPink{225, 22, 96};
        constexpr Rgb8 kViolet{92, 18, 130};

        for (std::size_t index = 0;
             index < count;
             ++index) {

            const std::uint8_t spatial =
                count <= 1
                    ? 0U
                    : static_cast<std::uint8_t>(
                          (index * 255ULL) /
                          (count - 1U));

            output.pixels[index] =
                spatial < 128U
                    ? blend(
                          kOrange,
                          kPink,
                          static_cast<std::uint8_t>(
                              spatial * 2U))
                    : blend(
                          kPink,
                          kViolet,
                          static_cast<std::uint8_t>(
                              (spatial - 128U) * 2U));
        }
        break;
    }

    case ManualLightingEffect::Candle: {
        constexpr Rgb8 kFlame{255, 116, 22};
        const std::uint64_t stepUs =
            animationPeriodUs(
                state.speed,
                220000ULL,
                60000ULL);

        const std::uint32_t tick =
            stepUs == 0
                ? 0U
                : static_cast<std::uint32_t>(
                      nowUs / stepUs);

        for (std::size_t index = 0;
             index < count;
             ++index) {

            const std::uint32_t noise =
                mix32(
                    tick * 0x9e3779b9U +
                    static_cast<std::uint32_t>(index));

            const std::uint8_t depth =
                static_cast<std::uint8_t>(
                    24U +
                    (
                        static_cast<std::uint16_t>(
                            state.intensity) *
                        72U
                    ) /
                        255U);

            const std::uint8_t flicker =
                static_cast<std::uint8_t>(
                    255U -
                    (
                        (noise & 0xFFU) *
                        depth
                    ) /
                        255U);

            output.pixels[index] =
                scaleColor(
                    kFlame,
                    flicker);
        }
        break;
    }

    case ManualLightingEffect::Aurora: {
        const std::uint64_t periodUs =
            animationPeriodUs(
                state.speed,
                18000000ULL,
                2200000ULL);

        const std::uint8_t base =
            phaseFor(
                nowUs,
                periodUs);

        for (std::size_t index = 0;
             index < count;
             ++index) {

            const std::uint8_t spatial =
                static_cast<std::uint8_t>(
                    (index * 384ULL) /
                    count);

            const std::uint8_t wave =
                triangle(
                    static_cast<std::uint8_t>(
                        base * 2U +
                        spatial));

            const Rgb8 color =
                auroraColor(
                    static_cast<std::uint8_t>(
                        base + spatial));

            const std::uint8_t floor =
                static_cast<std::uint8_t>(
                    255U -
                    state.intensity / 2U);

            const std::uint8_t level =
                static_cast<std::uint8_t>(
                    floor +
                    (
                        static_cast<std::uint16_t>(
                            255U - floor) *
                        wave
                    ) /
                        255U);

            output.pixels[index] =
                scaleColor(
                    color,
                    level);
        }
        break;
    }

    case ManualLightingEffect::Twinkle: {
        const std::uint64_t stepUs =
            animationPeriodUs(
                state.speed,
                480000ULL,
                90000ULL);

        const std::uint32_t tick =
            stepUs == 0
                ? 0U
                : static_cast<std::uint32_t>(
                      nowUs / stepUs);

        const std::uint8_t background =
            static_cast<std::uint8_t>(
                12U +
                (
                    static_cast<std::uint16_t>(
                        255U - state.intensity) *
                    44U
                ) /
                    255U);

        for (std::size_t index = 0;
             index < count;
             ++index) {

            const std::uint32_t noise =
                mix32(
                    tick * 0x85ebca6bU +
                    static_cast<std::uint32_t>(index));

            const bool sparkle =
                (noise & 0xFFU) <
                static_cast<std::uint32_t>(
                    8U + state.intensity / 8U);

            output.pixels[index] =
                scaleColor(
                    state.color,
                    sparkle
                        ? 255U
                        : background);
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
