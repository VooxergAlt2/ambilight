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

// Compatibility name for code/tests that specifically mean the default
// topology. Runtime code must use the active topology total instead.
constexpr std::size_t kLogicalLedCount =
    kDefaultLogicalLedCount;

constexpr std::array<std::uint8_t, kParlioLaneCount> kLedGpios = {
    18, // TOP
    19, // RIGHT
    20, // BOTTOM
    21  // LEFT
};

constexpr std::uint8_t kTofSdaGpio = 6;
constexpr std::uint8_t kTofSclGpio = 7;

// Conservative first-boot output limit. Runtime/NVS setting can change this
// without rebuilding firmware.
constexpr std::uint8_t kDefaultOutputBrightness = 32;

} // namespace ambilight::config
