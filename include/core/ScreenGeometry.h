#pragma once

#include <array>
#include <cstddef>

#include "core/Geometry.h"

namespace ambilight {

struct ScreenPointMm {
    float xMm = 0.0F;
    float yMm = 0.0F;
    float zMm = 0.0F;
};

struct SegmentScreenGeometry {
    SegmentId id = SegmentId::Top;
    ScreenPointMm logicalStart{};
    ScreenPointMm logicalEnd{};
};

using PerimeterScreenGeometry =
    std::array<
        SegmentScreenGeometry,
        static_cast<std::size_t>(SegmentId::Count)>;

} // namespace ambilight
