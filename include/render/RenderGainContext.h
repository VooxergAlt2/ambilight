#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "config/BoardConfig.h"
#include "core/GainQ12.h"
#include "core/HeapBuffer.h"
#include "core/Geometry.h"
#include "core/RgbFrame.h"
#include "led/LedMappingProfile.h"

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

    // Exact logical LED gain field. Storage follows the active topology and is
    // resized only on control-plane topology changes, never in the render hot
    // path. Index is HyperHDR logical perimeter index before physical mapping.
    HeapBuffer<std::uint16_t> logicalGainQ12{};

    LedMappingProfile topology{};

    bool sourcePresent = false;
    bool sourceUsable = false;
    bool failOpen = true;

    RenderGainContext() {
        configureTopology(
            LedMappingProfile{});
    }

    bool configureTopology(
        const LedMappingProfile& activeTopology) {

        if (!activeTopology.valid()) {
            return false;
        }

        HeapBuffer<std::uint16_t> candidate;

        if (!candidate.resize(
                activeTopology.totalLedCount(),
                kGainUnityQ12)) {

            return false;
        }

        logicalGainQ12.swap(candidate);
        topology = activeTopology;
        return true;
    }

    bool storageValid() const {
        return
            topology.valid() &&
            logicalGainQ12.size() >=
                topology.totalLedCount();
    }

    static RenderGainContext unity(
        const LedMappingProfile& activeTopology =
            LedMappingProfile{}) {

        RenderGainContext context;

        if (!context.configureTopology(
                activeTopology)) {

            context.logicalGainQ12.clearStorage();
            context.topology =
                activeTopology;
        }

        return context;
    }

    void forceUnity() {
        logicalGainQ12.fill(
            kGainUnityQ12);
    }

    std::uint16_t gainForLogicalIndex(
        std::size_t logicalIndex) const {

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

        const SegmentConfig configSegment =
            topology.segmentConfig(
                segment);

        if (configSegment.logicalLength == 0) {
            return {};
        }

        const std::size_t start =
            configSegment.logicalStart;

        const std::size_t end =
            configSegment.logicalStart +
            configSegment.logicalLength -
            1U;

        return SegmentGainEndpoints{
            gainForLogicalIndex(start),
            gainForLogicalIndex(end)
        };
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

        const SegmentConfig configSegment =
            topology.segmentConfig(
                segment);

        if (configSegment.logicalLength == 0 ||
            logicalGainQ12.size() <
                topology.totalLedCount()) {

            return;
        }

        {
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

        }
    }

    bool hasNonUnityGain() const {
        if (!sourceUsable ||
            failOpen ||
            !storageValid()) {
            return false;
        }

        const std::size_t count =
            topology.totalLedCount();

        for (std::size_t index = 0;
             index < count;
             ++index) {

            if (sanitizeGainQ12(
                    logicalGainQ12[index]) !=
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

        if (topology.segment !=
                other.topology.segment ||
            !storageValid() ||
            !other.storageValid()) {

            return false;
        }

        const std::size_t count =
            topology.totalLedCount();

        for (std::size_t index = 0;
             index < count;
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
        std::size_t logicalIndex,
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
