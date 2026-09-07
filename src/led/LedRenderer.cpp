#include "led/LedRenderer.h"

#include <algorithm>

#include <esp_timer.h>

namespace ambilight {

bool LedRenderer::setMappingProfile(
    const LedMappingProfile& profile) {

    LedRenderPlan candidate;

    if (!LedRenderPlan::build(
            profile,
            candidate)) {

        return false;
    }

    // PARLIO owns a fixed 230-pixel buffer for every physical lane. When
    // runtime topology shrinks a side or moves it to another lane, pixels
    // outside the new active span must be cleared before the new mapping can
    // be rendered. This is intentionally topology-time work, never frame-time.
    if (mappingProfile_.segment !=
        profile.segment) {

        engine_.clear();
    }

    mappingProfile_ =
        profile;

    renderPlan_ =
        candidate;

    // Keep the renderer internally valid even if a topology is applied before
    // its persisted mask is sanitized by the higher-level settings transaction.
    if (!pixelMaskProfile_.validFor(
            mappingProfile_)) {

        pixelMaskProfile_.
            sanitizeFor(
                mappingProfile_);
    }

    return true;
}

bool LedRenderer::setPixelMaskProfile(
    const LedPixelMaskProfile& profile) {

    if (!profile.validFor(
            mappingProfile_)) {

        return false;
    }

    pixelMaskProfile_ =
        profile;

    return true;
}

esp_err_t LedRenderer::render(
    const RgbFrame& frame) {

    return render(
        frame,
        RenderGainContext::unity(
            mappingProfile_),
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

    const std::uint64_t renderStartedUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    const std::uint64_t prepareStartedUs =
        renderStartedUs;

    std::uint16_t frameWouldChangePixels = 0;
    std::uint16_t framePhysicalChangedPixels = 0;
    std::uint8_t frameMaxChannelDelta = 0;

    std::uint32_t frameInputChannelSum = 0;
    std::uint32_t frameShadowChannelSum = 0;

    std::array<
        std::uint32_t,
        static_cast<std::size_t>(
            SegmentId::Count)>
        frameChangedBySegment{};

    const std::uint16_t activeLedCount =
        renderPlan_.totalLedCount;

    const auto& output =
        engine_.frameWriteView();

    if (!renderPlan_.valid ||
        !output.valid() ||
        frame.pixelCount !=
            activeLedCount) {

        ++mappingErrors_;
        return ESP_ERR_INVALID_ARG;
    }

    for (std::size_t segmentIndex = 0;
         segmentIndex <
            renderPlan_.segment.size();
         ++segmentIndex) {

        const auto& segment =
            renderPlan_.
                segment[
                    segmentIndex];

        if (segment.lane >=
                output.lane.size()) {

            ++mappingErrors_;
            return ESP_FAIL;
        }

        const auto& lane =
            output.lane[
                segment.lane];

        if (!lane.valid() ||
            segment.logicalLength >
                lane.pixelCount) {

            ++mappingErrors_;
            return ESP_FAIL;
        }

        const std::uint16_t disabledOffset =
            pixelMaskProfile_.
                disabledOffset[
                    segmentIndex];

        for (std::uint16_t offset = 0;
             offset <
                segment.logicalLength;
             ++offset) {

            const std::uint16_t logical =
                static_cast<std::uint16_t>(
                    segment.logicalStart +
                    offset);

            const std::uint16_t physical =
                segment.physicalIndex(
                    offset);

            const Rgb8& original =
                frame.pixels[
                    logical];

            const ShadowPixelResult shadow =
                RenderGainMath::preview(
                    original,
                    logical,
                    segment.id,
                    gainContext);

            frameInputChannelSum +=
                static_cast<std::uint32_t>(
                    original.r) +
                static_cast<std::uint32_t>(
                    original.g) +
                static_cast<std::uint32_t>(
                    original.b);

            frameShadowChannelSum +=
                static_cast<std::uint32_t>(
                    shadow.wouldOutput.r) +
                static_cast<std::uint32_t>(
                    shadow.wouldOutput.g) +
                static_cast<std::uint32_t>(
                    shadow.wouldOutput.b);

            if (shadow.wouldChange) {
                ++frameWouldChangePixels;

                ++frameChangedBySegment[
                    segmentIndex];
            }

            frameMaxChannelDelta =
                std::max(
                    frameMaxChannelDelta,
                    shadow.maxChannelDelta);

            Rgb8 physicalOutput =
                CorrectionOutputPolicy::
                    physicalOutput(
                        correctionMode,
                        original,
                        shadow);

            if (disabledOffset !=
                    LedPixelMaskProfile::kNone &&
                disabledOffset ==
                    offset) {

                physicalOutput = {};
            }

            if (physicalOutput.r !=
                    original.r ||
                physicalOutput.g !=
                    original.g ||
                physicalOutput.b !=
                    original.b) {

                ++framePhysicalChangedPixels;
            }

            lane.writeUnchecked(
                physical,
                physicalOutput);
        }
    }

    const std::uint64_t prepareFinishedUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    const std::uint32_t prepareUs =
        static_cast<std::uint32_t>(
            prepareFinishedUs -
            prepareStartedUs);

    prepareMetric_.observe(
        prepareUs);

    const esp_err_t result =
        engine_.show();

    if (result != ESP_OK) {
        return result;
    }

    const std::uint64_t postStartedUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

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
        ++shadowStats_.
            sourcePresentFrames;
    }

    if (gainContext.sourceUsable) {
        ++shadowStats_.
            sourceUsableFrames;
    }

    if (gainContext.failOpen) {
        ++shadowStats_.
            failOpenFrames;
    }

    if (gainContext.hasNonUnityGain()) {
        ++shadowStats_.
            nonUnityContextFrames;
    }

    shadowStats_.evaluatedPixels +=
        activeLedCount;

    shadowStats_.wouldChangePixels +=
        frameWouldChangePixels;

    shadowStats_.physicalChangedPixels +=
        framePhysicalChangedPixels;

    for (std::size_t index = 0;
         index <
            frameChangedBySegment.size();
         ++index) {

        shadowStats_.
            wouldChangeBySegment[
                index] +=
            frameChangedBySegment[
                index];
    }

    shadowStats_.maxChannelDelta =
        std::max(
            shadowStats_.
                maxChannelDelta,
            frameMaxChannelDelta);

    shadowStats_.
        lastWouldChangePixels =
        frameWouldChangePixels;

    shadowStats_.
        lastPhysicalChangedPixels =
        framePhysicalChangedPixels;

    shadowStats_.
        lastMaxChannelDelta =
        frameMaxChannelDelta;

    shadowStats_.
        lastInputChannelSum =
        frameInputChannelSum;

    shadowStats_.
        lastShadowChannelSum =
        frameShadowChannelSum;

    shadowStats_.
        lastSourceGeneration =
        gainContext.sourceGeneration;

    shadowStats_.
        lastSourceAgeUs =
        gainContext.sourceAgeUs;

    shadowStats_.
        lastPrepareUs =
        prepareUs;

    shadowStats_.
        maxPrepareUs =
        std::max(
            shadowStats_.
                maxPrepareUs,
            prepareUs);

    lastGainContext_ =
        gainContext;

    lastCorrectionMode_ =
        correctionMode;

    const std::uint64_t renderFinishedUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    postMetric_.observe(
        renderFinishedUs -
        postStartedUs);

    renderMetric_.observe(
        renderFinishedUs -
        renderStartedUs);

    return ESP_OK;
}

} // namespace ambilight
