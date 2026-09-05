#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

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

    // Endpoints live in LOGICAL segment order.
    //
    // Physical lane reversal happens later in SegmentMapper and must never
    // reverse the mathematical gain profile by accident.
    //
    // Future wall-plane logic can therefore create a gradient along a segment
    // without changing LedRenderer. Activation of physical gradients still
    // requires verified screen-space orientation for every logical segment.
    std::array<
        SegmentGainEndpoints,
        static_cast<std::size_t>(SegmentId::Count)>
        segmentGain{};

    bool sourcePresent = false;
    bool sourceUsable = false;
    bool failOpen = true;

    static constexpr RenderGainContext unity() {
        return {};
    }

    constexpr const SegmentGainEndpoints& endpointsForSegment(
        SegmentId segment) const {

        const auto index =
            static_cast<std::size_t>(segment);

        if (index >= segmentGain.size()) {
            return segmentGain[0];
        }

        return segmentGain[index];
    }

    constexpr std::uint16_t gainForPosition(
        SegmentId segment,
        std::uint16_t logicalOffset,
        std::uint16_t segmentLength) const {

        if (!sourceUsable || failOpen) {
            return kGainUnityQ12;
        }

        const auto& endpoints =
            endpointsForSegment(segment);

        const std::uint16_t start =
            sanitizeGainQ12(endpoints.startQ12);
        const std::uint16_t end =
            sanitizeGainQ12(endpoints.endQ12);

        if (segmentLength <= 1 ||
            logicalOffset == 0 ||
            start == end) {
            return start;
        }

        if (logicalOffset >= segmentLength - 1) {
            return end;
        }

        const std::int32_t span =
            static_cast<std::int32_t>(end) -
            static_cast<std::int32_t>(start);

        const std::uint32_t denominator =
            static_cast<std::uint32_t>(
                segmentLength - 1);

        const std::int64_t numerator =
            static_cast<std::int64_t>(span) *
            logicalOffset;

        const std::int64_t rounded =
            numerator >= 0
                ? numerator +
                    static_cast<std::int64_t>(
                        denominator / 2U)
                : numerator -
                    static_cast<std::int64_t>(
                        denominator / 2U);

        const std::int32_t interpolated =
            static_cast<std::int32_t>(start) +
            static_cast<std::int32_t>(
                rounded /
                static_cast<std::int64_t>(denominator));

        if (interpolated <= 0) {
            return 0;
        }

        if (interpolated >= kGainUnityQ12) {
            return kGainUnityQ12;
        }

        return static_cast<std::uint16_t>(
            interpolated);
    }

    constexpr bool hasNonUnityGain() const {
        if (!sourceUsable || failOpen) {
            return false;
        }

        for (const auto& gain : segmentGain) {
            if (!gain.isUnity()) {
                return true;
            }
        }

        return false;
    }

    // Compare only what can change rendered RGB.
    //
    // Generation/timestamp/age are diagnostics and do not make a profile
    // dirty by themselves. Usable vs fail-open state does matter even when
    // both currently resolve to unity, because reacquisition starts a new
    // slew from the safe unity state.
    constexpr bool sameRenderProfileAs(
        const RenderGainContext& other) const {

        const bool usable =
            sourceUsable && !failOpen;

        const bool otherUsable =
            other.sourceUsable && !other.failOpen;

        if (usable != otherUsable) {
            return false;
        }

        if (!usable) {
            return true;
        }

        for (std::size_t index = 0;
             index < segmentGain.size();
             ++index) {

            if (sanitizeGainQ12(
                    segmentGain[index].startQ12) !=
                sanitizeGainQ12(
                    other.segmentGain[index].startQ12)) {
                return false;
            }

            if (sanitizeGainQ12(
                    segmentGain[index].endQ12) !=
                sanitizeGainQ12(
                    other.segmentGain[index].endQ12)) {
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
            ? static_cast<std::uint8_t>(a - b)
            : static_cast<std::uint8_t>(b - a);
    }

    static constexpr ShadowPixelResult preview(
        const Rgb8& input,
        SegmentId segment,
        std::uint16_t logicalOffset,
        std::uint16_t segmentLength,
        const RenderGainContext& context) {

        ShadowPixelResult result;
        result.original = input;
        result.segment = segment;

        result.gainQ12 =
            context.gainForPosition(
                segment,
                logicalOffset,
                segmentLength);

        result.wouldOutput =
            apply(input, result.gainQ12);

        const std::uint8_t dr =
            absDiff(input.r, result.wouldOutput.r);
        const std::uint8_t dg =
            absDiff(input.g, result.wouldOutput.g);
        const std::uint8_t db =
            absDiff(input.b, result.wouldOutput.b);

        result.maxChannelDelta =
            dr > dg
                ? (dr > db ? dr : db)
                : (dg > db ? dg : db);

        result.wouldChange =
            result.maxChannelDelta != 0;

        return result;
    }
};

// Stage 11 safety policy.
//
// There is intentionally no runtime flag that can enable gain application.
// The shadow candidate is observable, but hardware output is compile-time
// defined as original HyperHDR RGB.
class ShadowRenderPolicy {
public:
    static constexpr Rgb8 physicalOutput(
        const Rgb8& original,
        const ShadowPixelResult&) {

        return original;
    }
};

} // namespace ambilight
