#include "led/LedRenderer.h"

#include <algorithm>

#include <esp_timer.h>

#include "led/SegmentMapper.h"

namespace ambilight {

crgb_t LedRenderer::toCrgb(const Rgb8& color) {
    return
        (static_cast<crgb_t>(color.r) << 16) |
        (static_cast<crgb_t>(color.g) << 8) |
        static_cast<crgb_t>(color.b);
}

esp_err_t LedRenderer::render(const RgbFrame& frame) {
    return render(
        frame,
        RenderGainContext::unity(),
        CorrectionMode::Disabled);
}

esp_err_t LedRenderer::render(
    const RgbFrame& frame,
    const RenderGainContext& gainContext) {

    return render(
        frame,
        gainContext,
        CorrectionMode::Shadow);
}

esp_err_t LedRenderer::render(
    const RgbFrame& frame,
    const RenderGainContext& gainContext,
    CorrectionMode correctionMode) {

    const std::uint64_t prepareStartedUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    std::uint16_t frameWouldChangePixels = 0;
    std::uint16_t framePhysicalChangedPixels = 0;
    std::uint8_t frameMaxChannelDelta = 0;

    std::uint32_t frameInputChannelSum = 0;
    std::uint32_t frameShadowChannelSum = 0;

    std::array<
        std::uint32_t,
        static_cast<std::size_t>(SegmentId::Count)>
        frameChangedBySegment{};

    for (std::uint16_t logical = 0;
         logical < config::kLogicalLedCount;
         ++logical) {

        const PhysicalPixel mapped =
            SegmentMapper::map(
                logical,
                mappingProfile_);

        if (!mapped.valid) {
            ++mappingErrors_;
            return ESP_ERR_INVALID_ARG;
        }

        const Rgb8& original =
            frame.pixels[logical];

        const ShadowPixelResult shadow =
            RenderGainMath::preview(
                original,
                logical,
                mapped.segment,
                gainContext);

        frameInputChannelSum +=
            static_cast<std::uint32_t>(original.r) +
            static_cast<std::uint32_t>(original.g) +
            static_cast<std::uint32_t>(original.b);

        frameShadowChannelSum +=
            static_cast<std::uint32_t>(shadow.wouldOutput.r) +
            static_cast<std::uint32_t>(shadow.wouldOutput.g) +
            static_cast<std::uint32_t>(shadow.wouldOutput.b);

        if (shadow.wouldChange) {
            ++frameWouldChangePixels;

            const auto segmentIndex =
                static_cast<std::size_t>(
                    mapped.segment);

            if (segmentIndex <
                frameChangedBySegment.size()) {
                ++frameChangedBySegment[
                    segmentIndex];
            }
        }

        frameMaxChannelDelta =
            std::max(
                frameMaxChannelDelta,
                shadow.maxChannelDelta);

        const Rgb8 physicalOutput =
            CorrectionOutputPolicy::physicalOutput(
                correctionMode,
                original,
                shadow);

        if (physicalOutput.r != original.r ||
            physicalOutput.g != original.g ||
            physicalOutput.b != original.b) {

            ++framePhysicalChangedPixels;
        }

        if (!engine_.setPhysicalPixel(
                mapped.lane,
                mapped.index,
                toCrgb(physicalOutput))) {
            ++mappingErrors_;
            return ESP_FAIL;
        }
    }

    const std::uint64_t prepareFinishedUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    const std::uint32_t prepareUs =
        static_cast<std::uint32_t>(
            prepareFinishedUs -
            prepareStartedUs);

    const esp_err_t result = engine_.show();
    if (result != ESP_OK) {
        return result;
    }

    ++renderedFrames_;

    ++shadowStats_.frames;

    switch (correctionMode) {
    case CorrectionMode::Disabled:
        ++shadowStats_.disabledFrames;
        break;
    case CorrectionMode::Shadow:
        ++shadowStats_.shadowFrames;
        break;
    case CorrectionMode::Active:
        ++shadowStats_.activeFrames;
        break;
    }

    if (gainContext.sourcePresent) {
        ++shadowStats_.sourcePresentFrames;
    }

    if (gainContext.sourceUsable) {
        ++shadowStats_.sourceUsableFrames;
    }

    if (gainContext.failOpen) {
        ++shadowStats_.failOpenFrames;
    }

    if (gainContext.hasNonUnityGain()) {
        ++shadowStats_.nonUnityContextFrames;
    }

    shadowStats_.evaluatedPixels +=
        config::kLogicalLedCount;

    shadowStats_.wouldChangePixels +=
        frameWouldChangePixels;

    shadowStats_.physicalChangedPixels +=
        framePhysicalChangedPixels;

    for (std::size_t index = 0;
         index < frameChangedBySegment.size();
         ++index) {

        shadowStats_.wouldChangeBySegment[index] +=
            frameChangedBySegment[index];
    }

    shadowStats_.maxChannelDelta =
        std::max(
            shadowStats_.maxChannelDelta,
            frameMaxChannelDelta);

    shadowStats_.lastWouldChangePixels =
        frameWouldChangePixels;

    shadowStats_.lastPhysicalChangedPixels =
        framePhysicalChangedPixels;

    shadowStats_.lastMaxChannelDelta =
        frameMaxChannelDelta;

    shadowStats_.lastInputChannelSum =
        frameInputChannelSum;

    shadowStats_.lastShadowChannelSum =
        frameShadowChannelSum;

    shadowStats_.lastSourceGeneration =
        gainContext.sourceGeneration;

    shadowStats_.lastSourceAgeUs =
        gainContext.sourceAgeUs;

    shadowStats_.lastPrepareUs =
        prepareUs;

    shadowStats_.maxPrepareUs =
        std::max(
            shadowStats_.maxPrepareUs,
            prepareUs);

    lastGainContext_ =
        gainContext;

    lastCorrectionMode_ =
        correctionMode;

    return ESP_OK;
}

} // namespace ambilight
