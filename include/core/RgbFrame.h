#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "config/BoardConfig.h"

namespace ambilight {

struct Rgb8 {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;

    constexpr bool operator==(const Rgb8& other) const {
        return r == other.r && g == other.g && b == other.b;
    }
};

struct RgbFrame {
    std::uint32_t generation = 0;
    std::uint64_t receivedUs = 0;
    std::array<Rgb8, config::kLogicalLedCount> pixels{};

    void clear() {
        pixels.fill(Rgb8{});
    }
};

static_assert(sizeof(Rgb8) == 3, "Rgb8 must stay packed as three bytes");
static_assert(
    config::kLogicalLedCount * sizeof(Rgb8) == 2340,
    "780 RGB LEDs must occupy 2340 payload bytes");

} // namespace ambilight
