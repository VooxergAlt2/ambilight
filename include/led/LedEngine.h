#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <LiteLED.h>
#include <esp_err.h>

#include "config/BoardConfig.h"

namespace ambilight {

// Thin owner of the ESP32-C6 PARLIO hardware.
//
// This class knows only physical lanes. Logical LED geometry belongs to
// SegmentMapper/LedRenderer.
class LedEngine {
public:
    LedEngine();

    esp_err_t begin();
    esp_err_t show();

    void setBrightness(
        std::uint8_t brightness);

    std::uint8_t brightness() const {
        return brightness_;
    }

    void clear();
    bool setPhysicalPixel(
        std::uint8_t lane,
        std::uint16_t physicalIndex,
        crgb_t color);

    std::uint32_t lastShowTimeUs() const { return lastShowTimeUs_; }
    std::uint32_t maxShowTimeUs() const { return maxShowTimeUs_; }

private:
    LiteLEDpioGroup group_;
    std::array<LiteLEDpioLane*, config::kParlioLaneCount> lanes_{};

    std::uint8_t brightness_ =
        config::kDefaultOutputBrightness;

    bool begun_ = false;

    std::uint32_t lastShowTimeUs_ = 0;
    std::uint32_t maxShowTimeUs_ = 0;
};

} // namespace ambilight
