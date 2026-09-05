#pragma once

#include <cstddef>

#include "tof/TofTypes.h"

namespace ambilight {

constexpr std::size_t tofRawIndexForNormalized(
    std::size_t normalizedRow,
    std::size_t normalizedCol,
    const TofGridTransform& transform) {

    std::size_t x = normalizedCol;
    std::size_t y = normalizedRow;

    if (transform.mirrorX) {
        x = kTofGridWidth - 1 - x;
    }

    std::size_t rawX = x;
    std::size_t rawY = y;

    switch (transform.rotation) {
    case TofRotation::Deg0:
        rawX = x;
        rawY = y;
        break;

    case TofRotation::Deg90:
        rawX = y;
        rawY = kTofGridHeight - 1 - x;
        break;

    case TofRotation::Deg180:
        rawX = kTofGridWidth - 1 - x;
        rawY = kTofGridHeight - 1 - y;
        break;

    case TofRotation::Deg270:
        rawX = kTofGridWidth - 1 - y;
        rawY = x;
        break;
    }

    return rawY * kTofGridWidth + rawX;
}

} // namespace ambilight
