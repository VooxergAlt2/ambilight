#pragma once

#include "core/ScreenGeometry.h"

namespace ambilight::config {

// Provisional LED rectangle derived from the selected strip lengths:
// TOP/BOTTOM: 230 LEDs / 160 LEDs per meter = 1437.5 mm
// LEFT/RIGHT: 160 LEDs / 160 LEDs per meter = 1000 mm
//
// These are calibration geometry values, not claims about the TV panel's
// manufacturer dimensions.
constexpr float kLedPerimeterWidthMm = 1437.5F;
constexpr float kLedPerimeterHeightMm = 1000.0F;

// Sensor position relative to the LED rectangle center.
// +X = right, +Y = up.
constexpr float kTofSensorOffsetXmm = 0.0F;
constexpr float kTofSensorOffsetYmm = 0.0F;

// LED emitter plane relative to the ToF optical origin.
// +Z points from TV toward wall.
constexpr float kLedPlaneZFromTofMm = 0.0F;

// Ignore sensor jitter that changes the predicted wall position by less than
// this amount everywhere on the LED rectangle. The comparison is cumulative
// against the last plane that actually rebuilt the gain field.
constexpr float kTofPlaneWallDeadbandMm = 10.0F;

constexpr float halfWidth() {
    return kLedPerimeterWidthMm / 2.0F;
}

constexpr float halfHeight() {
    return kLedPerimeterHeightMm / 2.0F;
}

constexpr ScreenPointMm fromScreenCenter(
    float xMm,
    float yMm) {

    return ScreenPointMm{
        xMm - kTofSensorOffsetXmm,
        yMm - kTofSensorOffsetYmm,
        kLedPlaneZFromTofMm
    };
}

// IMPORTANT: these are LOGICAL screen-space directions, independent from
// physical strip wiring reversal.
//
// Current provisional clockwise order:
// TOP:    left -> right
// RIGHT:  top -> bottom
// BOTTOM: right -> left
// LEFT:   bottom -> top
//
// Hardware acceptance must verify these directions before physical gains are
// ever enabled.
constexpr PerimeterScreenGeometry kPerimeterScreenGeometry = {{
    {
        SegmentId::Top,
        fromScreenCenter(-halfWidth(), +halfHeight()),
        fromScreenCenter(+halfWidth(), +halfHeight())
    },
    {
        SegmentId::Right,
        fromScreenCenter(+halfWidth(), +halfHeight()),
        fromScreenCenter(+halfWidth(), -halfHeight())
    },
    {
        SegmentId::Bottom,
        fromScreenCenter(+halfWidth(), -halfHeight()),
        fromScreenCenter(-halfWidth(), -halfHeight())
    },
    {
        SegmentId::Left,
        fromScreenCenter(-halfWidth(), -halfHeight()),
        fromScreenCenter(-halfWidth(), +halfHeight())
    }
}};

constexpr bool perimeterScreenGeometryIsValid() {
    for (std::size_t index = 0;
         index < kPerimeterScreenGeometry.size();
         ++index) {

        if (static_cast<std::size_t>(
                kPerimeterScreenGeometry[index].id) !=
            index) {
            return false;
        }
    }

    return
        kLedPerimeterWidthMm > 0.0F &&
        kLedPerimeterHeightMm > 0.0F;
}

static_assert(
    perimeterScreenGeometryIsValid(),
    "Screen perimeter geometry is invalid");

} // namespace ambilight::config
