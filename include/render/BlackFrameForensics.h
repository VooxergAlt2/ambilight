#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "core/GainQ12.h"
#include "core/RgbFrame.h"
#include "led/LedMappingProfile.h"
#include "led/LedPixelMaskProfile.h"
#include "render/RenderGainContext.h"

namespace ambilight {

enum class BlackFrameReason : std::uint8_t {
    None = 0,
    SourceBlack,
    ActiveGain,
    BrightnessZero,
    Unknown
};

inline const char* blackFrameReasonName(
    BlackFrameReason reason) {

    switch (reason) {
    case BlackFrameReason::SourceBlack:
        return "SOURCE_BLACK";
    case BlackFrameReason::ActiveGain:
        return "ACTIVE_GAIN";
    case BlackFrameReason::BrightnessZero:
        return "BRIGHTNESS_ZERO";
    case BlackFrameReason::Unknown:
        return "UNKNOWN";
    case BlackFrameReason::None:
    default:
        return "NONE";
    }
}

struct BlackFrameSample {
    std::uint32_t generation = 0;
    std::uint64_t observedUs = 0;
    std::uint64_t sourceAgeUs = 0;

    std::uint32_t pixelCount = 0;
    std::uint32_t sourceNonZeroPixels = 0;
    std::uint32_t afterGainNonZeroPixels = 0;
    std::uint32_t outputNonZeroPixels = 0;
    std::uint32_t zeroGainPixels = 0;

    std::uint8_t sourceMaxChannel = 0;
    std::uint8_t outputMaxChannel = 0;

    std::uint16_t gainMinQ12 = kGainUnityQ12;
    std::uint16_t gainMaxQ12 = kGainUnityQ12;

    bool activeCorrection = false;
    bool outputBlack = false;

    BlackFrameReason reason =
        BlackFrameReason::None;
};

struct BlackFrameForensicsStats {
    std::uint32_t framesObserved = 0;
    std::uint32_t invalidSamples = 0;

    std::uint32_t blackFrames = 0;
    std::uint32_t blackEvents = 0;
    std::uint32_t sourceBlackFrames = 0;
    std::uint32_t activeGainBlackFrames = 0;
    std::uint32_t brightnessZeroFrames = 0;
    std::uint32_t unknownBlackFrames = 0;

    std::uint32_t consecutiveBlackFrames = 0;
    std::uint32_t maxConsecutiveBlackFrames = 0;

    BlackFrameSample latest{};
    BlackFrameSample lastBlack{};
};

class BlackFrameForensics {
public:
    bool observe(
        const RgbFrame& frame,
        const LedMappingProfile& topology,
        const LedPixelMaskProfile& pixelMask,
        std::uint8_t brightness,
        const RenderGainContext* activeGain,
        std::uint64_t nowUs) {

        if (!topology.valid() ||
            !frame.storageValid() ||
            frame.pixelCount !=
                topology.totalLedCount() ||
            !pixelMask.validFor(
                topology) ||
            (
                activeGain != nullptr &&
                (
                    activeGain->topology.segment !=
                        topology.segment ||
                    !activeGain->storageValid()
                )
            )) {

            ++stats_.invalidSamples;
            return false;
        }

        BlackFrameSample sample;
        sample.generation =
            frame.generation;
        sample.observedUs =
            nowUs;
        sample.sourceAgeUs =
            frame.receivedUs != 0 &&
            nowUs >= frame.receivedUs
                ? nowUs - frame.receivedUs
                : 0;
        sample.pixelCount =
            static_cast<std::uint32_t>(
                frame.pixelCount);
        sample.activeCorrection =
            activeGain != nullptr;

        if (sample.activeCorrection) {
            sample.gainMinQ12 =
                std::numeric_limits<
                    std::uint16_t>::max();
            sample.gainMaxQ12 = 0;
        }

        for (std::size_t segmentIndex = 0;
             segmentIndex <
                topology.segment.size();
             ++segmentIndex) {

            const auto segmentId =
                static_cast<SegmentId>(
                    segmentIndex);

            const auto segment =
                topology.segmentConfig(
                    segmentId);

            for (std::uint16_t offset = 0;
                 offset <
                    segment.logicalLength;
                 ++offset) {

                const std::size_t logical =
                    segment.logicalStart +
                    offset;

                const Rgb8 source =
                    frame.pixels[
                        logical];

                const std::uint8_t sourceMax =
                    maxChannel(
                        source);

                if (sourceMax != 0) {
                    ++sample.
                        sourceNonZeroPixels;
                }

                sample.sourceMaxChannel =
                    std::max(
                        sample.sourceMaxChannel,
                        sourceMax);

                std::uint16_t gainQ12 =
                    kGainUnityQ12;

                if (activeGain != nullptr) {
                    gainQ12 =
                        activeGain->
                            gainForLogicalIndex(
                                logical);
                }

                gainQ12 =
                    sanitizeGainQ12(
                        gainQ12);

                if (gainQ12 == 0) {
                    ++sample.zeroGainPixels;
                }

                sample.gainMinQ12 =
                    std::min(
                        sample.gainMinQ12,
                        gainQ12);

                sample.gainMaxQ12 =
                    std::max(
                        sample.gainMaxQ12,
                        gainQ12);

                const Rgb8 afterGain =
                    activeGain != nullptr
                        ? RenderGainMath::apply(
                              source,
                              gainQ12)
                        : source;

                if (maxChannel(
                        afterGain) != 0) {

                    ++sample.
                        afterGainNonZeroPixels;
                }

                // A disabled physical LED is a hole in the wire address
                // space. Every logical pixel is remapped around that hole, so
                // masking never removes a logical pixel from the output model.
                const Rgb8 output =
                    afterGain;

                const std::uint8_t outputMax =
                    maxChannel(
                        output);

                if (outputMax != 0) {
                    ++sample.
                        outputNonZeroPixels;
                }

                sample.outputMaxChannel =
                    std::max(
                        sample.outputMaxChannel,
                        outputMax);
            }
        }

        if (!sample.activeCorrection) {
            sample.gainMinQ12 =
                kGainUnityQ12;
            sample.gainMaxQ12 =
                kGainUnityQ12;
            sample.zeroGainPixels = 0;
        }

        sample.outputBlack =
            brightness == 0 ||
            sample.outputNonZeroPixels == 0;

        sample.reason =
            classify(
                sample,
                brightness);

        commit(
            sample);

        return true;
    }

    const BlackFrameForensicsStats& stats() const {
        return stats_;
    }

    void reset() {
        stats_ = {};
    }

    // Separate source-owner epochs without erasing historical counters.
    // The next observed DDP frame starts a fresh black/non-black sequence.
    void breakSequence() {
        stats_.latest = {};
        stats_.consecutiveBlackFrames = 0;
    }

private:
    static std::uint8_t maxChannel(
        const Rgb8& value) {

        return
            std::max(
                value.r,
                std::max(
                    value.g,
                    value.b));
    }

    static BlackFrameReason classify(
        const BlackFrameSample& sample,
        std::uint8_t brightness) {

        if (!sample.outputBlack) {
            return BlackFrameReason::None;
        }

        if (brightness == 0) {
            return
                BlackFrameReason::
                    BrightnessZero;
        }

        if (sample.sourceNonZeroPixels == 0) {
            return
                BlackFrameReason::
                    SourceBlack;
        }

        if (sample.activeCorrection &&
            sample.afterGainNonZeroPixels == 0) {

            return
                BlackFrameReason::
                    ActiveGain;
        }

        return BlackFrameReason::Unknown;
    }

    void commit(
        const BlackFrameSample& sample) {

        const bool previousBlack =
            stats_.latest.outputBlack;

        ++stats_.framesObserved;
        stats_.latest = sample;

        if (!sample.outputBlack) {
            stats_.consecutiveBlackFrames = 0;
            return;
        }

        ++stats_.blackFrames;

        if (!previousBlack) {
            ++stats_.blackEvents;
        }

        ++stats_.consecutiveBlackFrames;
        stats_.maxConsecutiveBlackFrames =
            std::max(
                stats_.maxConsecutiveBlackFrames,
                stats_.consecutiveBlackFrames);

        stats_.lastBlack =
            sample;

        switch (sample.reason) {
        case BlackFrameReason::SourceBlack:
            ++stats_.sourceBlackFrames;
            break;
        case BlackFrameReason::ActiveGain:
            ++stats_.activeGainBlackFrames;
            break;
        case BlackFrameReason::BrightnessZero:
            ++stats_.brightnessZeroFrames;
            break;
        case BlackFrameReason::Unknown:
            ++stats_.unknownBlackFrames;
            break;
        case BlackFrameReason::None:
        default:
            break;
        }
    }

    BlackFrameForensicsStats stats_{};
};

} // namespace ambilight
