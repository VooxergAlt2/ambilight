#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "core/Geometry.h"
#include "led/LedMappingProfile.h"

namespace ambilight {

struct LedPixelMaskProfile {
    static constexpr std::uint16_t kSchemaVersion = 2;
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

    // Persisted offsets are PHYSICAL strip indices counted from the
    // controller/data-input end of each lane. Offset 0 therefore always means
    // the first physical LED on the wire, independent of logical REV/FWD.
    constexpr bool disabledPhysical(
        SegmentId segment,
        std::uint16_t physicalOffset) const {

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
                physicalOffset;
    }

    constexpr bool disabledLogical(
        SegmentId segment,
        std::uint16_t logicalOffset,
        const LedMappingProfile& topology) const {

        const std::size_t index =
            static_cast<std::size_t>(
                segment);

        if (index >=
                disabledOffset.size() ||
            index >=
                topology.segment.size()) {

            return false;
        }

        const auto& mapping =
            topology.segment[index];

        if (logicalOffset >=
            mapping.logicalLength) {

            return false;
        }

        const std::uint16_t physicalOffset =
            mapping.reversed != 0
                ? static_cast<std::uint16_t>(
                      mapping.logicalLength -
                      1U -
                      logicalOffset)
                : logicalOffset;

        return
            disabledPhysical(
                segment,
                physicalOffset);
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
