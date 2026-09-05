#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "config/BoardConfig.h"
#include "core/GainQ12.h"
#include "core/Geometry.h"
#include "core/RgbFrame.h"

namespace ambilight {

struct SegmentGainEndpoints {
    std::uint16_t startQ12 = kGainUnityQ12;
    std::uint16_t endQ12 = kGainUnityQ12;

    constexpr bool isUnity() const {
        return
            sanitizeGainQ12(startQ12) == kGainUnityQ12 &&
            sanitizeGainQ12(endQ12) == kGainUnityQ12;
    }
};

struct RenderGainContext {
    std::uint32_t sourceGeneration = 0;
    std::uint64_t sourceTimestampUs = 0;
    std::uint64_t sourceAgeUs = 0;

    // Exact logical LED gain field.
    //
    // Index is HyperHDR logical perimeter index, before physical lane mapping
    // or strip reversal. This keeps spatial correction independent from wiring.
    std::array<
        std::uint16_t,
        config::kLogicalLedCount>
        logicalGainQ12{};

    bool sourcePresent = false;
    bool sourceUsable = false;
    bool failOpen = true;

    RenderGainContext() {
        forceUnity();
    }

    static RenderGainContext unity() {
        return RenderGainContext{};
    }

    void forceUnity() {
        logicalGainQ12.fill(
            kGainUnityQ12);
    }

    std::uint16_t gainForLogicalIndex(
        std::uint16_t logicalIndex) const {

        if (!sourceUsable ||
            failOpen ||
            logicalIndex >=
                logicalGainQ12.size()) {

            return kGainUnityQ12;
        }

        return sanitizeGainQ12(
            logicalGainQ12[
                logicalIndex]);
    }

    SegmentGainEndpoints endpointsForSegment(
        SegmentId segment) const {

        for (const auto& configSegment :
             kSegments) {

            if (configSegment.id != segment ||
                configSegment.logicalLength == 0) {
                continue;
            }

            const std::uint16_t start =
                configSegment.logicalStart;

            const std::uint16_t end =
                static_cast<std::uint16_t>(
                    configSegment.logicalStart +
                    configSegment.logicalLength -
                    1);

            return SegmentGainEndpoints{
                gainForLogicalIndex(start),
                gainForLogicalIndex(end)
            };
        }

        return {};
    }

    // Backward-compatible helper used by diagnostics/tests.
    // The authoritative data source is logicalGainQ12.
    std::uint16_t gainForPosition(
        SegmentId segment,
        std::uint16_t logicalOffset,
        std::uint16_t) const {

        for (const auto& configSegment :
             kSegments) {

            if (configSegment.id != segment ||
                logicalOffset >=
                    configSegment.logicalLength) {
                continue;
            }

            return gainForLogicalIndex(
                static_cast<std::uint16_t>(
                    configSegment.logicalStart +
                    logicalOffset));
        }

        return kGainUnityQ12;
    }

    void setSegmentUniform(
        SegmentId segment,
        std::uint16_t gainQ12) {

        setSegmentLinear(
            segment,
            gainQ12,
            gainQ12);
    }

    void setSegmentLinear(
        SegmentId segment,
        std::uint16_t startQ12,
        std::uint16_t endQ12) {

        startQ12 =
            sanitizeGainQ12(
                startQ12);

        endQ12 =
            sanitizeGainQ12(
                endQ12);

        for (const auto& configSegment :
             kSegments) {

            if (configSegment.id != segment ||
                configSegment.logicalLength == 0) {
                continue;
            }

            const std::uint32_t denominator =
                configSegment.logicalLength > 1
                    ? configSegment.logicalLength - 1
                    : 1;

            const std::int32_t span =
                static_cast<std::int32_t>(
                    endQ12) -
                static_cast<std::int32_t>(
                    startQ12);

            for (std::uint16_t offset = 0;
                 offset <
                    configSegment.logicalLength;
                 ++offset) {

                const std::int64_t numerator =
                    static_cast<std::int64_t>(
                        span) *
                    offset;

                const std::int64_t rounded =
                    numerator >= 0
                        ? numerator +
                            static_cast<std::int64_t>(
                                denominator / 2U)
                        : numerator -
                            static_cast<std::int64_t>(
                                denominator / 2U);

                const std::int32_t value =
                    static_cast<std::int32_t>(
                        startQ12) +
                    static_cast<std::int32_t>(
                        rounded /
                        static_cast<std::int64_t>(
                            denominator));

                logicalGainQ12[
                    configSegment.logicalStart +
                    offset] =
                    sanitizeGainQ12(
                        value <= 0
                            ? 0
                            : static_cast<std::uint16_t>(
                                  value));

                if (offset + 1 ==
                    configSegment.logicalLength) {
                    break;
                }
            }

            return;
        }
    }

    bool hasNonUnityGain() const {
        if (!sourceUsable ||
            failOpen) {
            return false;
        }

        for (const auto gain :
             logicalGainQ12) {

            if (sanitizeGainQ12(gain) !=
                kGainUnityQ12) {
                return true;
            }
        }

        return false;
    }

    // Compare only data that can change rendered RGB.
    // Diagnostic metadata does not make a frame dirty.
    bool sameRenderProfileAs(
        const RenderGainContext& other) const {

        const bool usable =
            sourceUsable &&
            !failOpen;

        const bool otherUsable =
            other.sourceUsable &&
            !other.failOpen;

        if (usable != otherUsable) {
            return false;
        }

        if (!usable) {
            return true;
        }

        for (std::size_t index = 0;
             index <
                logicalGainQ12.size();
             ++index) {

            if (sanitizeGainQ12(
                    logicalGainQ12[index]) !=
                sanitizeGainQ12(
                    other.logicalGainQ12[
                        index])) {
                return false;
            }
        }

        return true;
    }
};

struct ShadowPixelResult {
    Rgb8 original{};
    Rgb8 wouldOutput{};

    SegmentId segment = SegmentId::Top;
    std::uint16_t gainQ12 = kGainUnityQ12;

    bool wouldChange = false;
    std::uint8_t maxChannelDelta = 0;
};

class RenderGainMath {
public:
    static constexpr Rgb8 apply(
        const Rgb8& input,
        std::uint16_t gainQ12) {

        return Rgb8{
            applyGainQ12(input.r, gainQ12),
            applyGainQ12(input.g, gainQ12),
            applyGainQ12(input.b, gainQ12)
        };
    }

    static constexpr std::uint8_t absDiff(
        std::uint8_t a,
        std::uint8_t b) {

        return a >= b
            ? static_cast<std::uint8_t>(
                  a - b)
            : static_cast<std::uint8_t>(
                  b - a);
    }

    static ShadowPixelResult preview(
        const Rgb8& input,
        std::uint16_t logicalIndex,
        SegmentId segment,
        const RenderGainContext& context) {

        ShadowPixelResult result;
        result.original = input;
        result.segment = segment;

        result.gainQ12 =
            context.gainForLogicalIndex(
                logicalIndex);

        result.wouldOutput =
            apply(
                input,
                result.gainQ12);

        const std::uint8_t dr =
            absDiff(
                input.r,
                result.wouldOutput.r);

        const std::uint8_t dg =
            absDiff(
                input.g,
                result.wouldOutput.g);

        const std::uint8_t db =
            absDiff(
                input.b,
                result.wouldOutput.b);

        result.maxChannelDelta =
            dr > dg
                ? (dr > db ? dr : db)
                : (dg > db ? dg : db);

        result.wouldChange =
            result.maxChannelDelta != 0;

        return result;
    }
};

} // namespace ambilight
