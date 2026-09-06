#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "core/Geometry.h"
#include "led/LedMappingProfile.h"

namespace ambilight {

struct LedPixelMaskProfile {
    static constexpr std::uint16_t kSchemaVersion = 1;
    static constexpr std::uint16_t kNone = 0xFFFFU;

    std::array<
        std::uint16_t,
        static_cast<std::size_t>(SegmentId::Count)>
        disabledOffset{{
            kNone, // TOP
            kNone, // RIGHT
            kNone, // BOTTOM
            kNone  // LEFT
        }};

    constexpr bool valid() const {
        for (const std::uint16_t value :
             disabledOffset) {

            if (value != kNone &&
                value >=
                    config::kPhysicalLaneLength) {

                return false;
            }
        }

        return true;
    }

    constexpr bool validFor(
        const LedMappingProfile& topology) const {

        if (!valid() ||
            !topology.valid()) {

            return false;
        }

        for (std::size_t index = 0;
             index <
                disabledOffset.size();
             ++index) {

            const std::uint16_t value =
                disabledOffset[index];

            if (value != kNone &&
                value >=
                    topology
                        .segment[index]
                        .logicalLength) {

                return false;
            }
        }

        return true;
    }

    void sanitizeFor(
        const LedMappingProfile& topology) {

        for (std::size_t index = 0;
             index <
                disabledOffset.size();
             ++index) {

            if (disabledOffset[index] !=
                    kNone &&
                disabledOffset[index] >=
                    topology
                        .segment[index]
                        .logicalLength) {

                disabledOffset[index] =
                    kNone;
            }
        }
    }

    constexpr bool disabled(
        SegmentId segment,
        std::uint16_t segmentOffset) const {

        const std::size_t index =
            static_cast<std::size_t>(
                segment);

        if (index >=
            disabledOffset.size()) {

            return false;
        }

        return
            disabledOffset[index] !=
                kNone &&
            disabledOffset[index] ==
                segmentOffset;
    }

    constexpr std::uint16_t forSegment(
        SegmentId segment) const {

        const std::size_t index =
            static_cast<std::size_t>(
                segment);

        return index <
                disabledOffset.size()
            ? disabledOffset[index]
            : kNone;
    }
};

static_assert(
    std::is_trivially_copyable<
        LedPixelMaskProfile>::value,
    "Persisted pixel-mask profile must remain trivially copyable");

static_assert(
    sizeof(LedPixelMaskProfile) <= 16,
    "Persisted pixel-mask profile unexpectedly grew");

} // namespace ambilight
