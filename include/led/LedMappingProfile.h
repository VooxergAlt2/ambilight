#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "config/BoardConfig.h"
#include "config/PanelConfig.h"
#include "core/Geometry.h"

namespace ambilight {

struct LedSegmentMapping {
    std::uint16_t logicalLength = 1;
    std::uint8_t lane = 0;
    std::uint8_t reversed = 0;

    constexpr bool operator==(
        const LedSegmentMapping& other) const {

        return
            logicalLength ==
                other.logicalLength &&
            lane == other.lane &&
            reversed == other.reversed;
    }

    constexpr bool operator!=(
        const LedSegmentMapping& other) const {

        return !(*this == other);
    }
};

constexpr std::array<
    LedSegmentMapping,
    static_cast<std::size_t>(SegmentId::Count)>
makeDefaultLedMappingSegments() {

    std::array<
        LedSegmentMapping,
        static_cast<std::size_t>(SegmentId::Count)>
        result{};

    for (std::size_t index = 0;
         index < result.size();
         ++index) {

        const auto& hardware =
            config::kPanelLedSegments[
                index];

        result[index] = {
            hardware.logicalLength,
            config::laneForLedGpio(
                hardware.gpio),
            static_cast<std::uint8_t>(
                hardware.reversed
                    ? 1
                    : 0)
        };
    }

    return result;
}

// Historical name retained to avoid needless churn. Schema 3 makes the
// hardware-measured panel wiring the canonical default and invalidates stale
// schema-2 topology persisted by older firmware.
struct LedMappingProfile {
    static constexpr std::uint16_t kSchemaVersion = 3;

    std::array<
        LedSegmentMapping,
        static_cast<std::size_t>(SegmentId::Count)>
        segment =
            makeDefaultLedMappingSegments();

    constexpr bool valid() const {
        std::array<
            bool,
            config::kParlioLaneCount>
            usedLane{};

        std::size_t total = 0;

        for (const auto& mapping :
             segment) {

            if (mapping.logicalLength == 0 ||
                mapping.logicalLength >
                    config::kPhysicalLaneLength) {

                return false;
            }

            total +=
                mapping.logicalLength;

            if (total >
                config::kLogicalLedCapacity) {

                return false;
            }

            if (mapping.lane >=
                config::kParlioLaneCount) {

                return false;
            }

            if (mapping.reversed > 1) {
                return false;
            }

            if (usedLane[
                    mapping.lane]) {

                return false;
            }

            usedLane[
                mapping.lane] = true;
        }

        for (const bool used :
             usedLane) {

            if (!used) {
                return false;
            }
        }

        return total > 0;
    }

    constexpr std::uint16_t totalLedCount() const {
        std::uint16_t total = 0;

        for (const auto& mapping :
             segment) {

            total = static_cast<std::uint16_t>(
                total +
                mapping.logicalLength);
        }

        return total;
    }

    constexpr LedSegmentMapping forSegment(
        SegmentId id) const {

        const std::size_t index =
            static_cast<std::size_t>(
                id);

        if (index >=
            segment.size()) {

            return {};
        }

        return segment[index];
    }

    constexpr SegmentConfig segmentConfig(
        SegmentId id) const {

        const std::size_t wanted =
            static_cast<std::size_t>(
                id);

        if (wanted >=
            segment.size()) {

            return {};
        }

        std::uint16_t logicalStart = 0;

        for (std::size_t index = 0;
             index < wanted;
             ++index) {

            logicalStart =
                static_cast<std::uint16_t>(
                    logicalStart +
                    segment[index]
                        .logicalLength);
        }

        const auto& runtime =
            segment[wanted];

        return SegmentConfig{
            id,
            logicalStart,
            runtime.logicalLength,
            runtime.lane,
            runtime.reversed != 0
        };
    }

    constexpr std::uint8_t gpioForSegment(
        SegmentId id) const {

        const auto mapping =
            forSegment(id);

        return mapping.lane <
                config::kLedGpios.size()
            ? config::kLedGpios[
                  mapping.lane]
            : 0xFFU;
    }

    static constexpr bool laneForGpio(
        std::uint16_t gpio,
        std::uint8_t& lane) {

        for (std::size_t index = 0;
             index <
                config::kLedGpios.size();
             ++index) {

            if (config::kLedGpios[index] ==
                gpio) {

                lane =
                    static_cast<std::uint8_t>(
                        index);

                return true;
            }
        }

        return false;
    }
};

static_assert(
    std::is_trivially_copyable<
        LedMappingProfile>::value,
    "Persisted LED topology profile must remain trivially copyable");

static_assert(
    sizeof(LedMappingProfile) <= 24,
    "Persisted LED topology profile unexpectedly grew");

static_assert(
    LedMappingProfile{}.totalLedCount() ==
        config::kDefaultLogicalLedCount,
    "Default LED topology total changed unexpectedly");

static_assert(
    LedMappingProfile{}.
        gpioForSegment(
            SegmentId::Top) == 20 &&
    LedMappingProfile{}.
        gpioForSegment(
            SegmentId::Right) == 19 &&
    LedMappingProfile{}.
        gpioForSegment(
            SegmentId::Bottom) == 21 &&
    LedMappingProfile{}.
        gpioForSegment(
            SegmentId::Left) == 18,
    "Default LED topology must match measured panel side-to-GPIO wiring");

static_assert(
    LedMappingProfile{}.
        forSegment(
            SegmentId::Top).
        reversed == 1 &&
    LedMappingProfile{}.
        forSegment(
            SegmentId::Right).
        reversed == 1 &&
    LedMappingProfile{}.
        forSegment(
            SegmentId::Bottom).
        reversed == 1 &&
    LedMappingProfile{}.
        forSegment(
            SegmentId::Left).
        reversed == 0,
    "Default LED directions must match measured panel wiring");

} // namespace ambilight
