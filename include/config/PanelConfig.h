#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "config/BoardConfig.h"
#include "core/Geometry.h"

namespace ambilight::config {

struct PanelLedSegment {
    std::uint16_t logicalLength = 0;
    std::uint8_t gpio = 0xFF;
    bool reversed = false;
};

// Hardware truth measured on the installed TV panel.
//
// Logical perimeter convention:
//   TOP -> RIGHT -> BOTTOM -> LEFT
//
// Physical wiring:
//   TOP    -> GPIO20, reversed
//   RIGHT  -> GPIO19, reversed
//   BOTTOM -> GPIO21, reversed
//   LEFT   -> GPIO18, forward
//
// Keep side-to-GPIO assignment and direction here only. Runtime mapping
// derives PARLIO lane indices from BoardConfig::kLedGpios.
constexpr std::array<
    PanelLedSegment,
    static_cast<std::size_t>(SegmentId::Count)>
    kPanelLedSegments{{
        {230, 20, true},  // TOP
        {160, 19, true},  // RIGHT
        {230, 21, true},  // BOTTOM
        {160, 18, false}  // LEFT
    }};

constexpr std::uint8_t laneForLedGpio(
    std::uint8_t gpio) {

    for (std::size_t lane = 0;
         lane < kLedGpios.size();
         ++lane) {

        if (kLedGpios[lane] ==
            gpio) {

            return
                static_cast<std::uint8_t>(
                    lane);
        }
    }

    return 0xFF;
}

constexpr PanelLedSegment panelLedSegment(
    SegmentId id) {

    const std::size_t index =
        static_cast<std::size_t>(
            id);

    return index <
            kPanelLedSegments.size()
        ? kPanelLedSegments[index]
        : PanelLedSegment{};
}

constexpr bool measuredPanelTopologyValid() {
    std::array<
        bool,
        kParlioLaneCount>
        usedLane{};

    std::size_t total = 0;

    for (const auto& segment :
         kPanelLedSegments) {

        if (segment.logicalLength == 0) {

            return false;
        }

        const std::uint8_t lane =
            laneForLedGpio(
                segment.gpio);

        if (lane >=
                kParlioLaneCount ||
            usedLane[lane]) {

            return false;
        }

        usedLane[lane] = true;
        total += segment.logicalLength;
    }

    return
        total ==
        kDefaultLogicalLedCount;
}

static_assert(
    measuredPanelTopologyValid(),
    "Measured panel LED topology must cover every PARLIO lane exactly once");

static_assert(
    laneForLedGpio(20) == 2 &&
    laneForLedGpio(19) == 1 &&
    laneForLedGpio(21) == 3 &&
    laneForLedGpio(18) == 0,
    "Measured side GPIOs must resolve to the expected PARLIO lanes");

} // namespace ambilight::config
