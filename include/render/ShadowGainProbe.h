#pragma once

#include <cstddef>
#include <cstdint>

#include "render/RenderGainContext.h"

namespace ambilight {

// Deterministic debug-only profile for validating shadow math on real
// HyperHDR content before physical calibration exists.
//
// The profile is deliberately aggressive so changed-pixel and gradient
// diagnostics are obvious. ShadowRenderPolicy still prevents physical output.
class ShadowGainProbe {
public:
    static RenderGainContext make(
        std::uint32_t generation,
        std::uint64_t nowUs) {

        RenderGainContext context;

        context.sourceGeneration = generation;
        context.sourceTimestampUs = nowUs;
        context.sourceAgeUs = 0;

        context.sourcePresent = true;
        context.sourceUsable = true;
        context.failOpen = false;

        // TOP: 100% -> 75%
        set(
            context,
            SegmentId::Top,
            4096,
            3072);

        // RIGHT: 75%
        set(
            context,
            SegmentId::Right,
            3072,
            3072);

        // BOTTOM: 50% -> 100%
        set(
            context,
            SegmentId::Bottom,
            2048,
            4096);

        // LEFT: 25%
        set(
            context,
            SegmentId::Left,
            1024,
            1024);

        return context;
    }

private:
    static void set(
        RenderGainContext& context,
        SegmentId segment,
        std::uint16_t startQ12,
        std::uint16_t endQ12) {

        context.setSegmentLinear(
            segment,
            startQ12,
            endQ12);
    }
};

} // namespace ambilight
