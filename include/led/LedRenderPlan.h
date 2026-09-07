#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/Geometry.h"
#include "led/LedMappingProfile.h"

namespace ambilight {

struct LedRenderSegment {
    SegmentId id = SegmentId::Top;
    std::uint16_t logicalStart = 0;
    std::uint16_t logicalLength = 0;
    std::uint8_t lane = 0;
    bool reversed = false;

    constexpr std::uint16_t physicalIndex(
        std::uint16_t segmentOffset) const {

        return reversed
            ? static_cast<std::uint16_t>(
                  logicalLength -
                  1U -
                  segmentOffset)
            : segmentOffset;
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

    std::uint16_t totalLedCount = 0;
    bool valid = false;

    static constexpr bool build(
        const LedMappingProfile& profile,
        LedRenderPlan& output) {

        output = {};

        if (!profile.valid()) {
            return false;
        }

        std::uint16_t logicalStart = 0;

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

            logicalStart =
                static_cast<std::uint16_t>(
                    logicalStart +
                    source.logicalLength);
        }

        output.totalLedCount =
            logicalStart;

        output.valid =
            logicalStart > 0;

        return output.valid;
    }
};

} // namespace ambilight
