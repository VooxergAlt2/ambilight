#pragma once

#include <cstdint>

namespace ambilight {

enum class CorrectionMode : std::uint8_t {
    Disabled = 0,
    Shadow = 1,
    Active = 2
};

constexpr bool correctionModeValid(
    std::uint8_t raw) {

    return raw <=
        static_cast<std::uint8_t>(
            CorrectionMode::Active);
}

constexpr const char* correctionModeName(
    CorrectionMode mode) {

    switch (mode) {
    case CorrectionMode::Disabled:
        return "DISABLED";
    case CorrectionMode::Shadow:
        return "SHADOW";
    case CorrectionMode::Active:
        return "ACTIVE";
    }

    return "INVALID";
}

} // namespace ambilight
