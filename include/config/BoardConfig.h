#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ambilight::config {

constexpr std::size_t kLogicalLedCount = 780;
constexpr std::size_t kParlioLaneCount = 4;
constexpr std::size_t kPhysicalLaneLength = 230;

// Provisional ESP32-C6-DevKitC-1 mapping.
// Keep native USB Serial/JTAG pins GPIO12/GPIO13 untouched for future work.
// Verify all pins against the exact production board before final wiring.
constexpr std::array<std::uint8_t, kParlioLaneCount> kLedGpios = {
    18, // TOP
    19, // RIGHT
    20, // BOTTOM
    21  // LEFT
};

// Provisional VL53L5CX I2C mapping for the development board.
// INT is intentionally not used in the Wi-Fi/DDP bring-up stage.
constexpr std::uint8_t kTofSdaGpio = 6;
constexpr std::uint8_t kTofSclGpio = 7;

// Early hardware stages intentionally run at low brightness. Power validation
// happens separately from signal/PARLIO validation.
constexpr std::uint8_t kTestBrightness = 32;

} // namespace ambilight::config
