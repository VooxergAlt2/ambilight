#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "config/BoardConfig.h"
#include "core/Geometry.h"

namespace ambilight {

struct LedSegmentMapping {
    std::uint8_t lane = 0;
    std::uint8_t reversed = 0;
};

struct LedMappingProfile {
    static constexpr std::uint16_t kSchemaVersion = 1;

    std::array<
        LedSegmentMapping,
        static_cast<std::size_t>(SegmentId::Count)>
        segment{{
            {0, 0}, // TOP
            {1, 0}, // RIGHT
            {2, 0}, // BOTTOM
            {3, 0}  // LEFT
        }};

    constexpr bool valid() const {
        std::array<
            bool,
            config::kParlioLaneCount>
            usedLane{};

        for (const auto& mapping :
             segment) {

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

        return true;
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
};

static_assert(
    std::is_trivially_copyable<
        LedMappingProfile>::value,
    "Persisted LED mapping profile must remain trivially copyable");

static_assert(
    sizeof(LedMappingProfile) <= 16,
    "Persisted LED mapping profile unexpectedly grew");

} // namespace ambilight
