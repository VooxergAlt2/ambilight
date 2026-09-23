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
        // Every stored value is representable by the persisted uint16 format.
        // kNone (0xFFFF) is the sentinel; topology-specific bounds are checked
        // by validFor()/apply().
        return true;
    }

    // A configured physical offset is a real hole in the serialized LED
    // chain, not a logical pixel painted black. The side therefore contains
    // logicalLength + 1 physical addresses and the disabled offset may be any
    // position in [0, logicalLength]. logicalLength==65535 cannot grow by one
    // because the driver/write-view representation is uint16_t.
    constexpr bool validFor(
        const LedMappingProfile& topology) const {

        if (!topology.valid()) {
            return false;
        }

        for (std::size_t index = 0;
             index < disabledOffset.size();
             ++index) {

            const std::uint16_t value =
                disabledOffset[index];

            if (value == kNone) {
                continue;
            }

            const std::uint16_t logicalLength =
                topology.segment[index].logicalLength;

            if (logicalLength >=
                    config::kMaxRepresentablePhysicalLaneLength ||
                value > logicalLength) {

                return false;
            }
        }

        return true;
    }

    constexpr void sanitizeFor(
        const LedMappingProfile& topology) {

        if (!topology.valid()) {
            *this = {};
            return;
        }

        for (std::size_t index = 0;
             index < disabledOffset.size();
             ++index) {

            if (disabledOffset[index] == kNone) {
                continue;
            }

            const std::uint16_t logicalLength =
                topology.segment[index].logicalLength;

            if (logicalLength >=
                    config::kMaxRepresentablePhysicalLaneLength ||
                disabledOffset[index] > logicalLength) {

                disabledOffset[index] = kNone;
            }
        }
    }

    // Persisted offsets are PHYSICAL strip addresses counted from the
    // controller/data-input end. Offset 0 is always the first physical LED on
    // the wire, independent of logical REV/FWD.
    constexpr bool disabledPhysical(
        SegmentId segment,
        std::uint16_t physicalOffset) const {

        const std::size_t index =
            static_cast<std::size_t>(segment);

        return
            index < disabledOffset.size() &&
            disabledOffset[index] != kNone &&
            disabledOffset[index] == physicalOffset;
    }

    constexpr bool hasHole(
        SegmentId segment) const {

        const std::size_t index =
            static_cast<std::size_t>(segment);

        return
            index < disabledOffset.size() &&
            disabledOffset[index] != kNone;
    }

    constexpr std::uint16_t physicalLengthForSegment(
        SegmentId segment,
        const LedMappingProfile& topology) const {

        const std::size_t index =
            static_cast<std::size_t>(segment);

        if (index >= topology.segment.size()) {
            return 0;
        }

        const std::uint16_t logicalLength =
            topology.segment[index].logicalLength;

        return
            hasHole(segment) &&
                    logicalLength <
                        config::kMaxRepresentablePhysicalLaneLength
                ? static_cast<std::uint16_t>(
                      logicalLength + 1U)
                : logicalLength;
    }

    constexpr std::size_t maxPhysicalLaneLength(
        const LedMappingProfile& topology) const {

        if (!validFor(topology)) {
            return 0;
        }

        std::size_t maximum = 0;

        for (std::size_t index = 0;
             index < disabledOffset.size();
             ++index) {

            const auto id =
                static_cast<SegmentId>(index);

            const std::size_t length =
                physicalLengthForSegment(
                    id,
                    topology);

            if (length > maximum) {
                maximum = length;
            }
        }

        return maximum;
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
