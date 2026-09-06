#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "config/BoardConfig.h"
#include "core/Geometry.h"

namespace ambilight {

struct LedSegmentMapping {
    std::uint16_t logicalLength = 1;
    std::uint8_t lane = 0;
    std::uint8_t reversed = 0;
};

// Historical name retained to avoid needless churn. Since schema 2 this is
// the authoritative runtime LED topology: logical side lengths plus the
// side-to-physical-lane assignment and direction.
struct LedMappingProfile {
    static constexpr std::uint16_t kSchemaVersion = 2;

    std::array<
        LedSegmentMapping,
        static_cast<std::size_t>(SegmentId::Count)>
        segment{{
            {230, 0, 0}, // TOP
            {160, 1, 0}, // RIGHT
            {230, 2, 0}, // BOTTOM
            {160, 3, 0}  // LEFT
        }};

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

} // namespace ambilight
