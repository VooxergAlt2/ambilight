#pragma once

#include <cstddef>
#include <cstdint>

#include "render/RenderGainContext.h"
#include "tof/TofGainModel.h"

namespace ambilight {

class TofRenderGainBridge {
public:
    static constexpr std::uint64_t kMaxGainSnapshotAgeUs =
        1500000;

    static constexpr RenderGainContext make(
        const GainSnapshot& gains,
        bool snapshotPresent,
        std::uint64_t nowUs) {

        RenderGainContext context;
        context.sourcePresent = snapshotPresent;

        if (!snapshotPresent) {
            return context;
        }

        context.sourceGeneration = gains.generation;
        context.sourceTimestampUs = gains.timestampUs;

        const bool timestampSane =
            gains.timestampUs != 0 &&
            nowUs >= gains.timestampUs;

        context.sourceAgeUs =
            timestampSane
                ? nowUs - gains.timestampUs
                : 0;

        const bool fresh =
            timestampSane &&
            context.sourceAgeUs <=
                kMaxGainSnapshotAgeUs;

        context.sourceUsable =
            fresh &&
            gains.geometryUsable &&
            !gains.failOpen;

        context.failOpen =
            !context.sourceUsable;

        if (!context.sourceUsable) {
            return context;
        }

        setUniform(
            context,
            SegmentId::Top,
            gains.topQ12);

        setUniform(
            context,
            SegmentId::Right,
            gains.rightQ12);

        setUniform(
            context,
            SegmentId::Bottom,
            gains.bottomQ12);

        setUniform(
            context,
            SegmentId::Left,
            gains.leftQ12);

        return context;
    }

private:
    static constexpr void setUniform(
        RenderGainContext& context,
        SegmentId segment,
        std::uint16_t gainQ12) {

        const auto index =
            static_cast<std::size_t>(segment);

        const std::uint16_t sanitized =
            sanitizeGainQ12(gainQ12);

        context.segmentGain[index].startQ12 =
            sanitized;
        context.segmentGain[index].endQ12 =
            sanitized;
    }
};

} // namespace ambilight
