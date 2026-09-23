#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/Geometry.h"
#include "led/LedMappingProfile.h"
#include "led/LedPixelMaskProfile.h"

namespace ambilight {

struct LedRenderSegment {
    SegmentId id = SegmentId::Top;
    std::size_t logicalStart = 0;
    std::uint16_t logicalLength = 0;
    std::uint8_t lane = 0;
    bool reversed = false;
    std::uint16_t disabledPhysicalOffset =
        LedPixelMaskProfile::kNone;
    std::uint16_t physicalLength = 0;

    constexpr std::uint16_t physicalIndex(
        std::uint16_t segmentOffset) const {

        const std::uint16_t wireOrdinal =
            reversed
                ? static_cast<std::uint16_t>(
                      logicalLength -
                      1U -
                      segmentOffset)
                : segmentOffset;

        return
            disabledPhysicalOffset !=
                    LedPixelMaskProfile::kNone &&
                wireOrdinal >=
                    disabledPhysicalOffset
                ? static_cast<std::uint16_t>(
                      wireOrdinal + 1U)
                : wireOrdinal;
    }
};

// Immutable-by-convention hot-path representation of LedMappingProfile.
//
// Runtime topology validation and logical-start accumulation happen when the
// profile is applied, never once per rendered pixel. LedRenderer stores one
// instance and iterates these four descriptors segment-major.
struct LedRenderPlan {
    std::array<
        LedRenderSegment,
        static_cast<std::size_t>(SegmentId::Count)>
        segment{};

    std::size_t totalLedCount = 0;
    bool valid = false;

    static bool build(
        const LedMappingProfile& profile,
        const LedPixelMaskProfile& mask,
        LedRenderPlan& output) {

        output = {};

        if (!profile.valid() ||
            !mask.validFor(profile)) {

            return false;
        }

        std::size_t logicalStart = 0;

        for (std::size_t index = 0;
             index < output.segment.size();
             ++index) {

            const auto& source =
                profile.segment[index];

            auto& destination =
                output.segment[index];

            destination.id =
                static_cast<SegmentId>(
                    index);

            destination.logicalStart =
                logicalStart;

            destination.logicalLength =
                source.logicalLength;

            destination.lane =
                source.lane;

            destination.reversed =
                source.reversed != 0;

            destination.disabledPhysicalOffset =
                mask.disabledOffset[index];

            destination.physicalLength =
                mask.physicalLengthForSegment(
                    destination.id,
                    profile);

            logicalStart +=
                source.logicalLength;
        }

        output.totalLedCount =
            logicalStart;

        output.valid =
            logicalStart > 0;

        return output.valid;
    }

    static bool build(
        const LedMappingProfile& profile,
        LedRenderPlan& output) {

        return
            build(
                profile,
                LedPixelMaskProfile{},
                output);
    }
};

} // namespace ambilight
