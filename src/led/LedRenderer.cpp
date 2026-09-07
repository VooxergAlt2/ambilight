#include "led/LedRenderer.h"

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
                    offset) {

                physicalOutput = {};
            }

            lane.writeUnchecked(
                physical,
                physicalOutput);
        }
    }

    return
        showPrepared(
            renderStartedUs,
            prepareStartedUs);
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
                    offset) {

                physicalOutput = {};
            }

            lane.writeUnchecked(
                physical,
                physicalOutput);
        }
    }

    return
        showPrepared(
            renderStartedUs,
            prepareStartedUs);
}

} // namespace ambilight
