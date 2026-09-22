#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ambilight::config {

constexpr std::size_t kParlioLaneCount = 4;

// Runtime topology has no fixed aggregate LED ceiling. Each persisted segment
// length is uint16_t, while total frame/storage size is derived dynamically
// from the active topology and ultimately limited by available controller RAM.
//
// Hardware validation baseline. This is not a software limit; it records the
// longest strip actually exercised on the reference installation.
constexpr std::size_t kTestedPhysicalLaneLength = 230;
constexpr std::size_t kDefaultPhysicalLaneLength =
    kTestedPhysicalLaneLength;

// Persisted/runtime lane lengths use uint16_t. This is a representation bound,
// not a project-level LED-count policy.
constexpr std::size_t kMaxRepresentablePhysicalLaneLength = 0xFFFFU;

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

// Commissioning patterns deliberately cap physical output below the normal
// runtime range. Keep this in shared configuration so serial, web status and
// runtime guards cannot drift apart.
constexpr std::uint8_t kCommissioningMaxBrightness = 64;

} // namespace ambilight::config
