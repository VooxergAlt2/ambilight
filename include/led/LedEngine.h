#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <LiteLED.h>
#include <esp_err.h>

#include "config/BoardConfig.h"
#include "core/PerformanceMetric.h"

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

    const PerformanceMetric& showMetric() const {
        return showMetric_;
    }

    // Compatibility accessors retained while Stage 40 replaces the old STAT
    // surface with percentile-based performance diagnostics.
    std::uint32_t lastShowTimeUs() const {
        return static_cast<std::uint32_t>(
            showMetric_.lastUs());
    }

    std::uint32_t maxShowTimeUs() const {
        return static_cast<std::uint32_t>(
            showMetric_.maxUs());
    }

private:
    LiteLEDpioGroup group_;
    std::array<LiteLEDpioLane*, config::kParlioLaneCount> lanes_{};

    std::uint8_t brightness_ =
        config::kDefaultOutputBrightness;

    bool begun_ = false;

    PerformanceMetric showMetric_{};
};

} // namespace ambilight
