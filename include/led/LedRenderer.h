#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <esp_err.h>

#include "core/RgbFrame.h"
#include "led/LedEngine.h"
#include "led/LedMappingProfile.h"
#include "led/LedPixelMaskProfile.h"
#include "render/CorrectionMode.h"
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
    std::uint64_t physicalChangedPixels = 0;

    std::uint32_t disabledFrames = 0;
    std::uint32_t shadowFrames = 0;
    std::uint32_t activeFrames = 0;

    std::array<
        std::uint64_t,
        static_cast<std::size_t>(SegmentId::Count)>
        wouldChangeBySegment{};

    std::uint8_t maxChannelDelta = 0;

    std::uint16_t lastWouldChangePixels = 0;
    std::uint16_t lastPhysicalChangedPixels = 0;
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

    bool setMappingProfile(
        const LedMappingProfile& profile) {

        if (!profile.valid()) {
            return false;
        }

        // PARLIO owns a fixed 230-pixel buffer for every physical lane.
        // When runtime topology shrinks a side or moves it to another lane,
        // pixels outside the new active span would otherwise retain their
        // previous RGB values and could reappear on the next show().
        //
        // Clear only on an actual topology change, never on every RGB frame.
        // Startup is safe too: LedEngine::clear() ignores lanes until begin().
        if (mappingProfile_.segment !=
            profile.segment) {

            engine_.clear();
        }

        mappingProfile_ = profile;
        return true;
    }

    const LedMappingProfile& mappingProfile() const {
        return mappingProfile_;
    }

    bool setPixelMaskProfile(
        const LedPixelMaskProfile& profile) {

        if (!profile.valid()) {
            return false;
        }

        pixelMaskProfile_ = profile;
        return true;
    }

    const LedPixelMaskProfile& pixelMaskProfile() const {
        return pixelMaskProfile_;
    }

    esp_err_t render(const RgbFrame& frame);

    // Backward-compatible preview path. Equivalent to SHADOW mode.
    esp_err_t render(
        const RgbFrame& frame,
        const RenderGainContext& gainContext);

    esp_err_t render(
        const RgbFrame& frame,
        const RenderGainContext& gainContext,
        CorrectionMode correctionMode);

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

    CorrectionMode lastCorrectionMode() const {
        return lastCorrectionMode_;
    }

private:
    static crgb_t toCrgb(const Rgb8& color);

    LedEngine& engine_;

    std::uint32_t renderedFrames_ = 0;
    std::uint32_t mappingErrors_ = 0;

    RenderShadowStats shadowStats_{};
    LedMappingProfile mappingProfile_{};
    LedPixelMaskProfile pixelMaskProfile_{};
    RenderGainContext lastGainContext_{};
    CorrectionMode lastCorrectionMode_ =
        CorrectionMode::Disabled;
};

} // namespace ambilight
