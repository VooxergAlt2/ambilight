#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ambilight::config {

constexpr std::size_t kLogicalLedCount = 780;
constexpr std::size_t kParlioLaneCount = 4;
constexpr std::size_t kPhysicalLaneLength = 230;

// Provisional ESP32-C6-DevKitC-1 mapping for Stage 1.
// Keep USB Serial/JTAG pins GPIO12/GPIO13 untouched.
// Verify these pins against the exact production board before final wiring.
constexpr std::array<std::uint8_t, kParlioLaneCount> kLedGpios = {
    18, // TOP
    19, // RIGHT
    20, // BOTTOM
    21  // LEFT
};

// Stage 1 intentionally runs at low brightness. Power validation happens
// separately from signal/PARLIO validation.
constexpr std::uint8_t kTestBrightness = 32;

} // namespace ambilight::config
