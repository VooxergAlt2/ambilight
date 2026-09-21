#include "led/LedRenderer.h"

#include <Arduino.h>
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

    if (mappingProfile_.segment !=
        profile.segment) {

        engine_.clear();
    }

    mappingProfile_ =
        profile;

    renderPlan_ =
        candidate;

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

bool LedRenderer::validateFrame(
    const RgbFrame& frame) const {

    return
        renderPlan_.valid &&
        engine_.frameWriteView().valid() &&
        frame.pixelCount ==
            renderPlan_.totalLedCount;
}

esp_err_t LedRenderer::showPrepared(
    std::uint64_t renderStartedUs,
    std::uint64_t prepareStartedUs) {

    const std::uint64_t prepareFinishedUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    prepareMetric_.observe(
        prepareFinishedUs -
        prepareStartedUs);

    const esp_err_t result =
        engine_.show();

    if (result != ESP_OK) {
        return result;
    }

    const std::uint64_t postStartedUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    ++renderedFrames_;

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

void LedRenderer::observeBlackFrameForensics(
    const RgbFrame& frame,
    const RenderGainContext* activeGain,
    std::uint64_t observedUs) {

    const auto& before =
        blackFrameForensics_.stats();

    const bool wasBlack =
        before.latest.outputBlack;

    const std::uint32_t previousEvents =
        before.blackEvents;

    const std::uint32_t previousRun =
        before.consecutiveBlackFrames;

    const std::uint64_t previousBlackUs =
        before.lastBlack.observedUs;

    if (!blackFrameForensics_.observe(
            frame,
            mappingProfile_,
            pixelMaskProfile_,
            engine_.brightness(),
            activeGain,
            observedUs)) {

        return;
    }

    const auto& after =
        blackFrameForensics_.stats();

    if (after.blackEvents !=
        previousEvents) {

        const auto& sample =
            after.lastBlack;

        Serial.printf(
            "BLACK EVENT reason=%s gen=%lu src_age_ms=%llu pixels=%u src_nonzero=%u src_max=%u after_gain_nonzero=%u out_nonzero=%u out_max=%u gain=%u..%u gain_zero=%u brightness=%u active=%s events=%lu\n",
            blackFrameReasonName(
                sample.reason),
            static_cast<unsigned long>(
                sample.generation),
            static_cast<unsigned long long>(
                sample.sourceAgeUs /
                1000ULL),
            static_cast<unsigned>(
                sample.pixelCount),
            static_cast<unsigned>(
                sample.sourceNonZeroPixels),
            static_cast<unsigned>(
                sample.sourceMaxChannel),
            static_cast<unsigned>(
                sample.afterGainNonZeroPixels),
            static_cast<unsigned>(
                sample.outputNonZeroPixels),
            static_cast<unsigned>(
                sample.outputMaxChannel),
            static_cast<unsigned>(
                sample.gainMinQ12),
            static_cast<unsigned>(
                sample.gainMaxQ12),
            static_cast<unsigned>(
                sample.zeroGainPixels),
            static_cast<unsigned>(
                engine_.brightness()),
            sample.activeCorrection
                ? "yes"
                : "no",
            static_cast<unsigned long>(
                after.blackEvents));

        return;
    }

    if (wasBlack &&
        !after.latest.outputBlack) {

        const std::uint64_t blackDurationUs =
            observedUs >= previousBlackUs
                ? observedUs -
                    previousBlackUs
                : 0;

        Serial.printf(
            "BLACK RECOVERY gen=%lu black_frames=%lu last_black_age_ms=%llu src_age_ms=%llu src_max=%u out_max=%u\n",
            static_cast<unsigned long>(
                frame.generation),
            static_cast<unsigned long>(
                previousRun),
            static_cast<unsigned long long>(
                blackDurationUs /
                1000ULL),
            static_cast<unsigned long long>(
                after.latest.sourceAgeUs /
                1000ULL),
            static_cast<unsigned>(
                after.latest.sourceMaxChannel),
            static_cast<unsigned>(
                after.latest.outputMaxChannel));
    }
}

esp_err_t LedRenderer::render(
    const RgbFrame& frame) {

    const std::uint64_t renderStartedUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    const std::uint64_t prepareStartedUs =
        renderStartedUs;

    if (!validateFrame(frame)) {
        ++mappingErrors_;
        return ESP_ERR_INVALID_ARG;
    }

    const auto& output =
        engine_.frameWriteView();

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

            Rgb8 physicalOutput =
                frame.pixels[
                    logical];

            if (disabledOffset !=
                    LedPixelMaskProfile::kNone &&
                disabledOffset ==
                    physical) {

                physicalOutput = {};
            }

            lane.writeUnchecked(
                physical,
                physicalOutput);
        }
    }

    const esp_err_t result =
        showPrepared(
            renderStartedUs,
            prepareStartedUs);

    if (result == ESP_OK) {
        observeBlackFrameForensics(
            frame,
            nullptr,
            static_cast<std::uint64_t>(
                esp_timer_get_time()));
    }

    return result;
}

esp_err_t LedRenderer::renderActive(
    const RgbFrame& frame,
    const RenderGainContext& gainContext) {

    const std::uint64_t renderStartedUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    const std::uint64_t prepareStartedUs =
        renderStartedUs;

    if (!validateFrame(frame) ||
        gainContext.topology.segment !=
            mappingProfile_.segment) {

        ++mappingErrors_;
        return ESP_ERR_INVALID_ARG;
    }

    const auto& output =
        engine_.frameWriteView();

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

            Rgb8 physicalOutput =
                RenderGainMath::apply(
                    frame.pixels[
                        logical],
                    gainContext.
                        gainForLogicalIndex(
                            logical));

            if (disabledOffset !=
                    LedPixelMaskProfile::kNone &&
                disabledOffset ==
                    physical) {

                physicalOutput = {};
            }

            lane.writeUnchecked(
                physical,
                physicalOutput);
        }
    }

    const esp_err_t result =
        showPrepared(
            renderStartedUs,
            prepareStartedUs);

    if (result == ESP_OK) {
        observeBlackFrameForensics(
            frame,
            &gainContext,
            static_cast<std::uint64_t>(
                esp_timer_get_time()));
    }

    return result;
}

} // namespace ambilight
