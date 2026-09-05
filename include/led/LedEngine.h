#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <LiteLED.h>
#include <esp_err.h>

#include "config/BoardConfig.h"

namespace ambilight {

enum class SegmentId : std::uint8_t {
    Top = 0,
    Right,
    Bottom,
    Left,
    Count
};

struct SegmentConfig {
    SegmentId id;
    std::uint16_t logicalStart;
    std::uint16_t logicalLength;
    std::uint8_t lane;
    std::uint8_t gpio;
    bool reversed;
};

class LedEngine {
public:
    LedEngine();

    esp_err_t begin();
    esp_err_t show();

    void clear();
    void fillSegment(SegmentId id, crgb_t color);
    bool setLogicalPixel(std::uint16_t logicalIndex, crgb_t color);

    void runIdentificationPattern(std::uint32_t holdMs);
    void runBoundaryPattern(std::uint32_t holdMs);
    void runLogicalWalk(std::uint32_t stepMs);
    void runFpsProbe(std::uint16_t targetFps, std::uint32_t durationMs);

    std::uint32_t lastShowTimeUs() const { return lastShowTimeUs_; }
    std::uint32_t maxShowTimeUs() const { return maxShowTimeUs_; }

    static const std::array<SegmentConfig, config::kParlioLaneCount>& segments();

private:
    static const SegmentConfig* findSegment(SegmentId id);

    LiteLEDpioGroup group_;
    std::array<LiteLEDpioLane*, config::kParlioLaneCount> lanes_{};

    std::uint32_t lastShowTimeUs_ = 0;
    std::uint32_t maxShowTimeUs_ = 0;
};

} // namespace ambilight
