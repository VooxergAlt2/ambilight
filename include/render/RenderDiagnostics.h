#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include "core/RgbFrame.h"
#include "led/LedPixelMaskProfile.h"
#include "led/LedRenderPlan.h"
#include "render/CorrectionMode.h"
#include "render/RenderGainContext.h"

namespace ambilight {

struct RenderDiagnosticStats {
    std::uint32_t diagnosticFrames = 0;
    std::uint32_t shadowFrames = 0;
    std::uint32_t activeFrames = 0;

    std::uint32_t sourcePresentFrames = 0;
    std::uint32_t sourceUsableFrames = 0;
    std::uint32_t failOpenFrames = 0;
    std::uint32_t nonUnityContextFrames = 0;

    std::uint64_t evaluatedPixels = 0;
    std::uint64_t wouldChangePixels = 0;
    std::uint64_t physicalChangedPixels = 0;

    std::array<
        std::uint64_t,
        static_cast<std::size_t>(
            SegmentId::Count)>
        wouldChangeBySegment{};

    std::uint8_t maxChannelDelta = 0;

    std::uint32_t lastWouldChangePixels = 0;
    std::uint32_t lastPhysicalChangedPixels = 0;
    std::uint8_t lastMaxChannelDelta = 0;

    std::uint32_t lastInputChannelSum = 0;
    std::uint32_t lastCandidateChannelSum = 0;

    std::uint32_t lastSourceGeneration = 0;
    std::uint64_t lastSourceAgeUs = 0;

    CorrectionMode lastMode =
        CorrectionMode::Shadow;
};

// Expensive correction diagnostics intentionally live outside LedRenderer.
// Callers decide the sampling cadence. The physical RGB hot path therefore
// does not pay gain-preview, delta and channel-sum costs on every video frame.
class RenderDiagnostics {
public:
    bool analyze(
        const RgbFrame& frame,
        const RenderGainContext& gainContext,
        CorrectionMode mode,
        const LedRenderPlan& plan,
        const LedPixelMaskProfile& mask) {

        if (mode == CorrectionMode::Disabled ||
            !plan.valid ||
            !frame.storageValid() ||
            frame.pixelCount !=
                plan.totalLedCount ||
            !gainContext.storageValid() ||
            !mask.validFor(
                gainContext.topology)) {

            return false;
        }

        std::uint32_t wouldChangePixels = 0;
        std::uint32_t physicalChangedPixels = 0;
        std::uint8_t maxChannelDelta = 0;

        std::uint32_t inputChannelSum = 0;
        std::uint32_t candidateChannelSum = 0;

        std::array<
            std::uint32_t,
            static_cast<std::size_t>(
                SegmentId::Count)>
            changedBySegment{};

        for (std::size_t segmentIndex = 0;
             segmentIndex <
                plan.segment.size();
             ++segmentIndex) {

            const auto& segment =
                plan.segment[
                    segmentIndex];

            const std::uint16_t disabledOffset =
                mask.disabledOffset[
                    segmentIndex];

            for (std::uint16_t offset = 0;
                 offset <
                    segment.logicalLength;
                 ++offset) {

                const std::size_t logical =
                    segment.logicalStart +
                    offset;

                const Rgb8& original =
                    frame.pixels[
                        logical];

                const ShadowPixelResult preview =
                    RenderGainMath::preview(
                        original,
                        logical,
                        segment.id,
                        gainContext);

                inputChannelSum +=
                    static_cast<std::uint32_t>(
                        original.r) +
                    static_cast<std::uint32_t>(
                        original.g) +
                    static_cast<std::uint32_t>(
                        original.b);

                candidateChannelSum +=
                    static_cast<std::uint32_t>(
                        preview.wouldOutput.r) +
                    static_cast<std::uint32_t>(
                        preview.wouldOutput.g) +
                    static_cast<std::uint32_t>(
                        preview.wouldOutput.b);

                if (preview.wouldChange) {
                    ++wouldChangePixels;
                    ++changedBySegment[
                        segmentIndex];
                }

                maxChannelDelta =
                    std::max(
                        maxChannelDelta,
                        preview.maxChannelDelta);

                Rgb8 physical =
                    mode ==
                            CorrectionMode::Active
                        ? preview.wouldOutput
                        : original;

                if (disabledOffset !=
                        LedPixelMaskProfile::kNone &&
                    disabledOffset ==
                        offset) {

                    physical = {};
                }

                if (physical.r != original.r ||
                    physical.g != original.g ||
                    physical.b != original.b) {

                    ++physicalChangedPixels;
                }
            }
        }

        ++stats_.diagnosticFrames;

        if (mode == CorrectionMode::Shadow) {
            ++stats_.shadowFrames;
        } else {
            ++stats_.activeFrames;
        }

        if (gainContext.sourcePresent) {
            ++stats_.sourcePresentFrames;
        }

        if (gainContext.sourceUsable) {
            ++stats_.sourceUsableFrames;
        }

        if (gainContext.failOpen) {
            ++stats_.failOpenFrames;
        }

        if (gainContext.hasNonUnityGain()) {
            ++stats_.nonUnityContextFrames;
        }

        stats_.evaluatedPixels +=
            plan.totalLedCount;

        stats_.wouldChangePixels +=
            wouldChangePixels;

        stats_.physicalChangedPixels +=
            physicalChangedPixels;

        for (std::size_t index = 0;
             index <
                changedBySegment.size();
             ++index) {

            stats_.wouldChangeBySegment[
                index] +=
                changedBySegment[
                    index];
        }

        stats_.maxChannelDelta =
            std::max(
                stats_.maxChannelDelta,
                maxChannelDelta);

        stats_.lastWouldChangePixels =
            wouldChangePixels;

        stats_.lastPhysicalChangedPixels =
            physicalChangedPixels;

        stats_.lastMaxChannelDelta =
            maxChannelDelta;

        stats_.lastInputChannelSum =
            inputChannelSum;

        stats_.lastCandidateChannelSum =
            candidateChannelSum;

        stats_.lastSourceGeneration =
            gainContext.sourceGeneration;

        stats_.lastSourceAgeUs =
            gainContext.sourceAgeUs;

        stats_.lastMode =
            mode;

        lastGainContext_ =
            gainContext;

        return true;
    }

    const RenderDiagnosticStats& stats() const {
        return stats_;
    }

    const RenderGainContext& lastGainContext() const {
        return lastGainContext_;
    }

private:
    RenderDiagnosticStats stats_{};
    RenderGainContext lastGainContext_{};
};

} // namespace ambilight
