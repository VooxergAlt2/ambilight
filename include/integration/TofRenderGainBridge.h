#pragma once

#include <cstddef>
#include <cstdint>

#include "render/RenderGainContext.h"
#include "tof/TofGainModel.h"
#include "tof/TofPerimeterGainModel.h"

namespace ambilight {

class TofRenderGainBridge {
public:
    static constexpr std::uint64_t kMaxGainSnapshotAgeUs =
        30000000;

    static RenderGainContext make(
        const PerimeterGainSnapshot& gains,
        bool snapshotPresent,
        std::uint64_t nowUs,
        const LedMappingProfile& activeTopology =
            LedMappingProfile{}) {

        RenderGainContext context =
            RenderGainContext::unity(
                activeTopology);

        context.sourcePresent = snapshotPresent;

        if (!snapshotPresent) {
            return context;
        }

        context.sourceGeneration =
            gains.generation;

        context.sourceTimestampUs =
            gains.timestampUs;

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
            gains.planeUsable &&
            gains.projectionUsable &&
            !gains.failOpen;

        context.failOpen =
            !context.sourceUsable;

        if (!context.sourceUsable) {
            return context;
        }

        context.topology =
            gains.topology;

        for (std::size_t index = 0;
             index <
                context.logicalGainQ12.size();
             ++index) {

            context.logicalGainQ12[index] =
                sanitizeGainQ12(
                    gains.logicalGainQ12[index]);
        }

        return context;
    }

    static RenderGainContext make(
        const GainSnapshot& gains,
        bool snapshotPresent,
        std::uint64_t nowUs,
        const LedMappingProfile& activeTopology =
            LedMappingProfile{}) {

        RenderGainContext context =
            RenderGainContext::unity(
                activeTopology);

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
    static void setUniform(
        RenderGainContext& context,
        SegmentId segment,
        std::uint16_t gainQ12) {

        context.setSegmentUniform(
            segment,
            gainQ12);
    }
};

} // namespace ambilight
