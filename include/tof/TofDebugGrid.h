#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "tof/TofGrid.h"
#include "tof/TofTypes.h"

namespace ambilight {

struct TofDebugZone {
    std::int16_t distanceMm = 0;
    std::uint8_t status = 0;
    std::uint8_t rawIndex = 0;

    constexpr bool usable() const {
        return
            status == 5 ||
            status == 6 ||
            status == 9;
    }

    constexpr bool fullConfidence() const {
        return status == 5;
    }
};

using TofDebugGrid =
    std::array<
        TofDebugZone,
        kTofZoneCount>;

inline TofDebugGrid makeTofDebugGrid(
    const TofRawFrame& raw,
    const TofGridTransform& transform) {

    TofDebugGrid result{};

    for (std::size_t row = 0;
         row < kTofGridHeight;
         ++row) {

        for (std::size_t col = 0;
             col < kTofGridWidth;
             ++col) {

            const std::size_t normalizedIndex =
                row *
                    kTofGridWidth +
                col;

            const std::size_t rawIndex =
                tofRawIndexForNormalized(
                    row,
                    col,
                    transform);

            auto& zone =
                result[
                    normalizedIndex];

            zone.distanceMm =
                raw.distanceMm[
                    rawIndex];

            zone.status =
                raw.targetStatus[
                    rawIndex];

            zone.rawIndex =
                static_cast<std::uint8_t>(
                    rawIndex);
        }
    }

    return result;
}

} // namespace ambilight
