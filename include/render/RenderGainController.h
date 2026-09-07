#pragma once

#include <cstddef>
#include <cstdint>

#include "render/RenderGainContext.h"

namespace ambilight {

struct RenderGainControllerConfig {
    // Full-scale 0.0 -> 1.0 transition takes about 500 ms.
    //
    // This shapes only valid gain changes. Fail-open always returns to unity
    // immediately so stale attenuation can never linger.
    std::uint32_t slewQ12PerSecond = 8192;
};

struct RenderGainControllerStats {
    std::uint32_t targetUpdates = 0;
    std::uint32_t advances = 0;

    std::uint32_t usableTargets = 0;
    std::uint32_t failOpenTargets = 0;
    std::uint32_t targetGenerationChanges = 0;
    std::uint32_t renderProfileChanges = 0;

    std::uint32_t failOpenUnitySnaps = 0;
    std::uint32_t timeRollbacks = 0;

    std::uint16_t maxPixelStepQ12 = 0;
};

class RenderGainController {
public:
    explicit RenderGainController(
        RenderGainControllerConfig config = {})
        : config_(config) {}

    // Target updates are intentionally infrequent. This is the only place
    // where the full target render profile is compared/copied.
    //
    // Returns true when the target data that can affect rendered RGB changed.
    bool setTarget(
        const RenderGainContext& target,
        std::uint64_t nowUs);

    // Advance current gains toward the stored target using elapsed real time.
    // The hot loop touches only active topology LEDs and is skipped entirely
    // while settled.
    //
    // Returns true when at least one active gain changed.
    bool advance(
        std::uint64_t nowUs);

    void reset(
        const LedMappingProfile& topology =
            LedMappingProfile{});

    const RenderGainContext& current() const {
        return current_;
    }

    const RenderGainContext& target() const {
        return target_;
    }

    const RenderGainControllerStats& stats() const {
        return stats_;
    }

    bool settled() const {
        return settled_;
    }

    bool nonUnity() const {
        return nonUnity_;
    }

private:
    static std::uint16_t moveTowards(
        std::uint16_t current,
        std::uint16_t target,
        std::uint16_t maxStep);

    std::uint16_t maxStepForDeltaTime(
        std::uint64_t deltaUs) const;

    void copyTargetMetadataToCurrent();

    RenderGainControllerConfig config_{};

    RenderGainContext current_{};
    RenderGainContext target_{};

    bool initialized_ = false;
    bool settled_ = true;
    bool nonUnity_ = false;

    std::uint64_t lastUpdateUs_ = 0;
    std::uint32_t lastTargetGeneration_ = 0;

    RenderGainControllerStats stats_{};
};

} // namespace ambilight
