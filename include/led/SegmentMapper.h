#pragma once

#include <cstdint>

#include "core/Geometry.h"
#include "led/LedMappingProfile.h"

namespace ambilight {

struct PhysicalPixel {
    std::uint8_t lane = 0;
    std::uint16_t index = 0;

    SegmentId segment = SegmentId::Top;
    std::uint16_t segmentOffset = 0;
    std::uint16_t segmentLength = 0;

    bool valid = false;
};

class SegmentMapper {
public:
    static constexpr PhysicalPixel mapInSegment(
        const SegmentConfig& segment,
        std::uint16_t logicalIndex) {

        const auto end = static_cast<std::uint16_t>(
            segment.logicalStart + segment.logicalLength);

        if (logicalIndex < segment.logicalStart || logicalIndex >= end) {
            return {};
        }

        const auto offset = static_cast<std::uint16_t>(
            logicalIndex - segment.logicalStart);

        const auto physicalIndex = segment.reversed
            ? static_cast<std::uint16_t>(
                  segment.logicalLength - 1 - offset)
            : offset;

        return PhysicalPixel{
            segment.lane,
            physicalIndex,
            segment.id,
            offset,
            segment.logicalLength,
            true
        };
    }

    static constexpr PhysicalPixel map(
        std::uint16_t logicalIndex,
        const LedMappingProfile& profile) {

        if (logicalIndex >= config::kLogicalLedCount ||
            !profile.valid()) {
            return {};
        }

        for (const auto& baseSegment : kSegments) {
            SegmentConfig segment =
                baseSegment;

            const auto runtime =
                profile.forSegment(
                    segment.id);

            segment.lane =
                runtime.lane;

            segment.reversed =
                runtime.reversed != 0;

            const PhysicalPixel mapped = mapInSegment(
                segment,
                logicalIndex);

            if (mapped.valid) {
                return mapped;
            }
        }

        return {};
    }

    static constexpr PhysicalPixel map(
        std::uint16_t logicalIndex) {

        return map(
            logicalIndex,
            LedMappingProfile{});
    }
};

} // namespace ambilight
