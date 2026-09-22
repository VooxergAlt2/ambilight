#pragma once

#include <cstddef>
#include <cstdint>

#include "config/BoardConfig.h"
#include "core/HeapBuffer.h"

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
    explicit RgbFrame(
        std::size_t count =
            config::kDefaultLogicalLedCount)
        : pixelCount(count),
          pixels(count) {}

    std::uint32_t generation = 0;
    std::uint64_t receivedUs = 0;

    std::size_t pixelCount =
        config::kDefaultLogicalLedCount;

    HeapBuffer<Rgb8> pixels{};

    bool resizePixels(
        std::size_t count) {

        if (count == 0) {
            return false;
        }

        if (!pixels.resize(
                count,
                Rgb8{})) {

            return false;
        }

        pixelCount = count;
        return true;
    }

    bool storageValid() const {
        return
            pixelCount > 0 &&
            pixels.size() >=
                pixelCount;
    }

    void clear() {
        pixels.fill(Rgb8{});
    }
};

static_assert(sizeof(Rgb8) == 3, "Rgb8 must stay packed as three bytes");
static_assert(
    config::kDefaultLogicalLedCount *
        sizeof(Rgb8) ==
        2340,
    "Default 780 RGB LEDs must occupy 2340 payload bytes");

} // namespace ambilight
