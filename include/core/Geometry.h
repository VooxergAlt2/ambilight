#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "config/BoardConfig.h"

namespace ambilight {

enum class SegmentId : std::uint8_t {
    Top = 0,
    Right,
    Bottom,
    Left,
    Count
};

struct SegmentConfig {
    SegmentId id;
    std::uint16_t logicalStart;
    std::uint16_t logicalLength;
    std::uint8_t lane;
    bool reversed;
};

constexpr std::array<SegmentConfig, config::kParlioLaneCount> kSegments = {{
    {SegmentId::Top,    0,   230, 0, false},
    {SegmentId::Right,  230, 160, 1, false},
    {SegmentId::Bottom, 390, 230, 2, false},
    {SegmentId::Left,   620, 160, 3, false},
}};

constexpr bool geometryIsValid() {
    std::uint16_t expectedStart = 0;

    for (std::size_t index = 0; index < kSegments.size(); ++index) {
        const auto& segment = kSegments[index];

        if (segment.logicalStart != expectedStart) {
            return false;
        }

        if (segment.logicalLength == 0 ||
            segment.logicalLength > config::kPhysicalLaneLength) {
            return false;
        }

        if (segment.lane >= config::kParlioLaneCount) {
            return false;
        }

        expectedStart = static_cast<std::uint16_t>(
            segment.logicalStart + segment.logicalLength);
    }

    return expectedStart == config::kLogicalLedCount;
}

static_assert(geometryIsValid(), "Logical segment geometry is invalid");

} // namespace ambilight
