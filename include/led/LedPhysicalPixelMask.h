#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "config/BoardConfig.h"
#include "core/RgbFrame.h"
#include "led/LedFrameWriteView.h"
#include "led/LedMappingProfile.h"
#include "led/LedPixelMaskProfile.h"

namespace ambilight {

// Physical lane-level mask enforced immediately before LED encoding.
//
// Unlike LedPixelMaskProfile, this representation is keyed by PARLIO lane and
// therefore applies equally to logical DDP renders, ACTIVE ToF renders and
// raw commissioning writes.
struct LedPhysicalPixelMask {
    static constexpr std::uint16_t kNone =
        LedPixelMaskProfile::kNone;

    std::array<
        std::uint16_t,
        config::kParlioLaneCount>
        disabledOffset{{
            kNone,
            kNone,
            kNone,
            kNone
        }};

    constexpr bool valid() const {
        for (const std::uint16_t offset :
             disabledOffset) {

            if (offset != kNone &&
                offset >=
                    config::kPhysicalLaneLength) {

                return false;
            }
        }

        return true;
    }

    static constexpr bool project(
        const LedMappingProfile& topology,
        const LedPixelMaskProfile& logicalMask,
        LedPhysicalPixelMask& output) {

        output = {};

        if (!topology.valid() ||
            !logicalMask.validFor(
                topology)) {

            return false;
        }

        for (std::size_t segment = 0;
             segment <
                topology.segment.size();
             ++segment) {

            const auto& mapping =
                topology.segment[
                    segment];

            if (mapping.lane >=
                output.disabledOffset.size()) {

                return false;
            }

            output.disabledOffset[
                mapping.lane] =
                    logicalMask.
                        disabledOffset[
                            segment];
        }

        return output.valid();
    }

    bool apply(
        const LedFrameWriteView& frame) const {

        if (!valid() ||
            !frame.valid()) {

            return false;
        }

        for (std::size_t lane = 0;
             lane <
                disabledOffset.size();
             ++lane) {

            const std::uint16_t offset =
                disabledOffset[
                    lane];

            if (offset == kNone) {
                continue;
            }

            const auto& target =
                frame.lane[
                    lane];

            if (!target.valid() ||
                offset >=
                    target.pixelCount) {

                return false;
            }

            target.writeUnchecked(
                offset,
                Rgb8{});
        }

        return true;
    }
};

} // namespace ambilight
