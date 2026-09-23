#pragma once

#include <cstdint>

#include "render/ManualLighting.h"

namespace ambilight {

enum class LightingOwner : std::uint8_t {
    Ddp = 0,
    Local = 1
};

class LightingSourcePolicy {
public:
    // Long enough to ignore an isolated transport hiccup, while still making
    // PC shutdown / HyperHDR stop feel immediate to a human observer.
    static constexpr std::uint64_t kDefaultFallbackTimeoutUs =
        1500000ULL;

    static bool ddpFresh(
        std::uint64_t nowUs,
        std::uint64_t lastCompleteFrameUs,
        std::uint64_t timeoutUs =
            kDefaultFallbackTimeoutUs) {

        return
            lastCompleteFrameUs != 0 &&
            nowUs >= lastCompleteFrameUs &&
            nowUs - lastCompleteFrameUs <=
                timeoutUs;
    }

    static LightingOwner resolve(
        const ManualLightingState& state,
        std::uint64_t nowUs,
        std::uint64_t lastCompleteFrameUs,
        std::uint64_t timeoutUs =
            kDefaultFallbackTimeoutUs) {

        if (state.effect !=
            ManualLightingEffect::Ambilight) {

            return LightingOwner::Local;
        }

        return
            ddpFresh(
                nowUs,
                lastCompleteFrameUs,
                timeoutUs)
                ? LightingOwner::Ddp
                : LightingOwner::Local;
    }

    static ManualLightingEffect localEffect(
        const ManualLightingState& state) {

        if (state.effect !=
            ManualLightingEffect::Ambilight) {

            return state.effect;
        }

        return
            ManualLighting::validLocalEffect(
                state.fallbackEffect)
                ? state.fallbackEffect
                : ManualLightingEffect::BiasWhite;
    }
};

} // namespace ambilight
