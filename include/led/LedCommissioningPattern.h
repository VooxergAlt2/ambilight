#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/Geometry.h"
#include "core/RgbFrame.h"

namespace ambilight {

enum class LedCommissioningPattern : std::uint8_t {
    None = 0,
    SegmentIdentity = 1,
    DirectionMarkers = 2
};

class LedCommissioningPatternBuilder {
public:
    static void build(
        LedCommissioningPattern pattern,
        RgbFrame& frame) {

        frame.clear();

        switch (pattern) {
        case LedCommissioningPattern::SegmentIdentity:
            buildSegmentIdentity(frame);
            break;

        case LedCommissioningPattern::DirectionMarkers:
            buildDirectionMarkers(frame);
            break;

        case LedCommissioningPattern::None:
            break;
        }
    }

private:
    static constexpr std::array<Rgb8, 4>
        kSegmentColors{{
            {255, 0, 0},       // TOP red
            {0, 255, 0},       // RIGHT green
            {0, 0, 255},       // BOTTOM blue
            {255, 255, 255}    // LEFT white
        }};

    static void buildSegmentIdentity(
        RgbFrame& frame) {

        for (const auto& segment :
             kSegments) {

            const auto color =
                kSegmentColors[
                    static_cast<std::size_t>(
                        segment.id)];

            for (std::uint16_t offset = 0;
                 offset <
                    segment.logicalLength;
                 ++offset) {

                frame.pixels[
                    segment.logicalStart +
                    offset] = color;
            }
        }
    }

    static void buildDirectionMarkers(
        RgbFrame& frame) {

        constexpr Rgb8 background{
            8, 8, 8
        };

        constexpr Rgb8 startColor{
            255, 0, 0
        };

        constexpr Rgb8 middleColor{
            0, 255, 0
        };

        constexpr Rgb8 endColor{
            0, 0, 255
        };

        constexpr std::uint16_t markerLength =
            5;

        for (const auto& segment :
             kSegments) {

            for (std::uint16_t offset = 0;
                 offset <
                    segment.logicalLength;
                 ++offset) {

                frame.pixels[
                    segment.logicalStart +
                    offset] =
                    background;
            }

            for (std::uint16_t offset = 0;
                 offset < markerLength &&
                 offset <
                    segment.logicalLength;
                 ++offset) {

                frame.pixels[
                    segment.logicalStart +
                    offset] =
                    startColor;

                frame.pixels[
                    segment.logicalStart +
                    segment.logicalLength -
                    1 -
                    offset] =
                    endColor;
            }

            const std::uint16_t middle =
                static_cast<std::uint16_t>(
                    segment.logicalStart +
                    segment.logicalLength /
                        2U);

            for (std::int16_t delta = -2;
                 delta <= 2;
                 ++delta) {

                const std::int32_t index =
                    static_cast<std::int32_t>(
                        middle) +
                    delta;

                if (index >=
                        segment.logicalStart &&
                    index <
                        segment.logicalStart +
                        segment.logicalLength) {

                    frame.pixels[
                        static_cast<std::size_t>(
                            index)] =
                        middleColor;
                }
            }
        }
    }
};

} // namespace ambilight
