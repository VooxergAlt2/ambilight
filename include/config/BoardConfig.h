#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ambilight::config {

constexpr std::size_t kParlioLaneCount = 4;
constexpr std::size_t kPhysicalLaneLength = 230;

// Static memory capacity. Runtime topology can use any valid total up to this
// value without heap allocation.
constexpr std::size_t kLogicalLedCapacity =
    kParlioLaneCount * kPhysicalLaneLength;

// Default 65-inch topology preserved from earlier stages.
constexpr std::size_t kDefaultLogicalLedCount = 780;

// PARLIO lane-to-GPIO wiring on the ESP32-C6 board. This table describes
// controller lanes only. Installed TV side assignment/direction belongs to
// PanelConfig.h and is derived from measured panel wiring.
constexpr std::array<std::uint8_t, kParlioLaneCount> kLedGpios = {
    18, // lane 0
    19, // lane 1
    20, // lane 2
    21  // lane 3
};

constexpr std::uint8_t kTofSdaGpio = 6;
constexpr std::uint8_t kTofSclGpio = 7;

// Conservative first-boot output limit. Runtime/NVS setting can change this
// without rebuilding firmware.
constexpr std::uint8_t kDefaultOutputBrightness = 32;

} // namespace ambilight::config
