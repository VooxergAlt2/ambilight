#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <esp_err.h>

#include "core/RgbFrame.h"
#include "led/LedEngine.h"
#include "render/RenderGainContext.h"

namespace ambilight {

struct RenderShadowStats {
    std::uint32_t frames = 0;

    std::uint32_t sourcePresentFrames = 0;
    std::uint32_t sourceUsableFrames = 0;
    std::uint32_t failOpenFrames = 0;
    std::uint32_t nonUnityContextFrames = 0;

    std::uint64_t evaluatedPixels = 0;
    std::uint64_t wouldChangePixels = 0;

    std::array<
        std::uint64_t,
        static_cast<std::size_t>(SegmentId::Count)>
        wouldChangeBySegment{};

    std::uint8_t maxChannelDelta = 0;

    std::uint16_t lastWouldChangePixels = 0;
    std::uint8_t lastMaxChannelDelta = 0;

    std::uint32_t lastInputChannelSum = 0;
    std::uint32_t lastShadowChannelSum = 0;

    std::uint32_t lastSourceGeneration = 0;
    std::uint64_t lastSourceAgeUs = 0;

    std::uint32_t lastPrepareUs = 0;
    std::uint32_t maxPrepareUs = 0;
};

class LedRenderer {
public:
    explicit LedRenderer(LedEngine& engine)
        : engine_(engine) {}

    esp_err_t render(const RgbFrame& frame);

    // Stage 11 shadow path.
    //
    // Gain math is evaluated for every logical pixel. ShadowRenderPolicy
    // guarantees that the actual physical output remains original RGB.
    esp_err_t render(
        const RgbFrame& frame,
        const RenderGainContext& gainContext);

    std::uint32_t renderedFrames() const {
        return renderedFrames_;
    }

    std::uint32_t mappingErrors() const {
        return mappingErrors_;
    }

    const RenderShadowStats& shadowStats() const {
        return shadowStats_;
    }

    const RenderGainContext& lastGainContext() const {
        return lastGainContext_;
    }

private:
    static crgb_t toCrgb(const Rgb8& color);

    LedEngine& engine_;

    std::uint32_t renderedFrames_ = 0;
    std::uint32_t mappingErrors_ = 0;

    RenderShadowStats shadowStats_{};
    RenderGainContext lastGainContext_{};
};

} // namespace ambilight
