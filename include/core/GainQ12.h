#pragma once

#include <algorithm>
#include <cstdint>

namespace ambilight {

constexpr std::uint16_t kGainUnityQ12 = 4096;

constexpr std::uint16_t sanitizeGainQ12(
    std::uint16_t gainQ12) {

    return gainQ12 > kGainUnityQ12
        ? kGainUnityQ12
        : gainQ12;
}

constexpr std::uint8_t applyGainQ12(
    std::uint8_t channel,
    std::uint16_t gainQ12) {

    const std::uint32_t sanitized =
        sanitizeGainQ12(gainQ12);

    // Round to nearest instead of truncating so small channel values do not
    // accumulate a systematic downward bias.
    const std::uint32_t scaled =
        static_cast<std::uint32_t>(channel) *
            sanitized +
        kGainUnityQ12 / 2U;

    return static_cast<std::uint8_t>(
        scaled / kGainUnityQ12);
}

} // namespace ambilight
