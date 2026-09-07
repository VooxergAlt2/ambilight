#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "config/BoardConfig.h"
#include "core/RgbFrame.h"

namespace ambilight {

// Normalized physical pixel-buffer contract used by Ambilight.
//
// LedEngine verifies the underlying driver is RGB, 3 bytes/pixel and GRB,
// then exposes it through this type. Render code therefore performs a direct
// three-byte store without depending on LiteLED internals or color-order
// switches.
struct LedLaneWriteView {
    std::uint8_t* grb = nullptr;
    std::uint16_t pixelCount = 0;

    bool valid() const {
        return
            grb != nullptr &&
            pixelCount > 0;
    }

    void writeUnchecked(
        std::uint16_t physicalIndex,
        const Rgb8& color) const {

        std::uint8_t* pixel =
            grb +
            static_cast<std::size_t>(
                physicalIndex) *
            3U;

        pixel[0] = color.g;
        pixel[1] = color.r;
        pixel[2] = color.b;
    }

    bool fill(
        std::uint16_t start,
        std::uint16_t count,
        const Rgb8& color) const {

        if (!valid() ||
            count == 0 ||
            start >= pixelCount ||
            static_cast<std::uint32_t>(
                start) +
                count >
                    pixelCount) {

            return false;
        }

        for (std::uint16_t offset = 0;
             offset < count;
             ++offset) {

            writeUnchecked(
                static_cast<std::uint16_t>(
                    start +
                    offset),
                color);
        }

        return true;
    }
};

struct LedFrameWriteView {
    std::array<
        LedLaneWriteView,
        config::kParlioLaneCount>
        lane{};

    bool valid() const {
        for (const auto& current :
             lane) {

            if (!current.valid()) {
                return false;
            }
        }

        return true;
    }
};

} // namespace ambilight
