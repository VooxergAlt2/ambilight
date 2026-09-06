#pragma once

#include <cstdint>

namespace ambilight {

enum class SegmentId : std::uint8_t {
    Top = 0,
    Right,
    Bottom,
    Left,
    Count
};

struct SegmentConfig {
    SegmentId id = SegmentId::Top;
    std::uint16_t logicalStart = 0;
    std::uint16_t logicalLength = 0;
    std::uint8_t lane = 0;
    bool reversed = false;
};

} // namespace ambilight
