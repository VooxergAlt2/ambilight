#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ambilight::config {

constexpr std::size_t kLogicalLedCount = 780;
constexpr std::size_t kParlioLaneCount = 4;
constexpr std::size_t kPhysicalLaneLength = 230;

constexpr std::array<std::uint8_t, kParlioLaneCount> kLedGpios = {
    18, // TOP
    19, // RIGHT
    20, // BOTTOM
    21  // LEFT
};

constexpr std::uint8_t kTofSdaGpio = 6;
constexpr std::uint8_t kTofSclGpio = 7;

// Raw ST zone orientation must be confirmed on the actual mounted sensor.
// Stage 8 can normalize all four rotations and an optional horizontal mirror
// without changing filtering code.
constexpr std::uint8_t kTofRotationQuarterTurns = 0;
constexpr bool kTofMirrorX = false;

// Conservative first-boot output limit. Runtime/NVS setting can change this
// without rebuilding firmware.
constexpr std::uint8_t kDefaultOutputBrightness = 32;

} // namespace ambilight::config
