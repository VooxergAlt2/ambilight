#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/RgbFrame.h"
#include "led/LedMappingProfile.h"

namespace ambilight {

enum class LedCommissioningPattern : std::uint8_t {
    None = 0,
    SegmentIdentity = 1,
    DirectionMarkers = 2,
    LogicalRange = 3,
    RawPhysicalRange = 4
};

class LedCommissioningPatternBuilder {
public:
    static void build(
        LedCommissioningPattern pattern,
        const LedMappingProfile& topology,
        RgbFrame& frame) {

        frame.clear();
        frame.pixelCount =
            topology.totalLedCount();

        switch (pattern) {
        case LedCommissioningPattern::SegmentIdentity:
            buildSegmentIdentity(
                topology,
                frame);
            break;

        case LedCommissioningPattern::DirectionMarkers:
            buildDirectionMarkers(
                topology,
                frame);
            break;

        case LedCommissioningPattern::None:
        case LedCommissioningPattern::LogicalRange:
        case LedCommissioningPattern::RawPhysicalRange:
            break;
        }
    }

    static void build(
        LedCommissioningPattern pattern,
        RgbFrame& frame) {

        build(
            pattern,
            LedMappingProfile{},
            frame);
    }

    static bool buildLogicalRange(
        const LedMappingProfile& topology,
        SegmentId segmentId,
        std::uint16_t startOffset,
        std::uint16_t count,
        RgbFrame& frame,
        Rgb8 color = {255, 255, 255}) {

        if (!topology.valid() ||
            count == 0) {

            return false;
        }

        const SegmentConfig segment =
            topology.segmentConfig(
                segmentId);

        const std::uint32_t end =
            static_cast<std::uint32_t>(
                startOffset) +
            count;

        if (startOffset >=
                segment.logicalLength ||
            end >
                segment.logicalLength) {

            return false;
        }

        frame.clear();
        frame.pixelCount =
            topology.totalLedCount();

        for (std::uint16_t offset = 0;
             offset < count;
             ++offset) {

            frame.pixels[
                segment.logicalStart +
                startOffset +
                offset] =
                color;
        }

        return true;
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
        const LedMappingProfile& topology,
        RgbFrame& frame) {

        for (std::size_t index = 0;
             index <
                static_cast<std::size_t>(
                    SegmentId::Count);
             ++index) {

            const auto segment =
                topology.segmentConfig(
                    static_cast<SegmentId>(
                        index));

            const auto color =
                kSegmentColors[index];

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
        const LedMappingProfile& topology,
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

        for (std::size_t segmentIndex = 0;
             segmentIndex <
                static_cast<std::size_t>(
                    SegmentId::Count);
             ++segmentIndex) {

            const auto segment =
                topology.segmentConfig(
                    static_cast<SegmentId>(
                        segmentIndex));

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
