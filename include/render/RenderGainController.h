#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "render/RenderGainContext.h"

namespace ambilight {

struct RenderGainControllerConfig {
    // Full-scale 0.0 -> 1.0 transition takes about 500 ms.
    //
    // This only shapes valid gain changes. Fail-open always returns to unity
    // immediately so stale attenuation can never linger.
    std::uint32_t slewQ12PerSecond = 8192;
};

struct RenderGainControllerStats {
    std::uint32_t updates = 0;
    std::uint32_t usableTargets = 0;
    std::uint32_t failOpenTargets = 0;
    std::uint32_t targetGenerationChanges = 0;
    std::uint32_t failOpenUnitySnaps = 0;
    std::uint32_t timeRollbacks = 0;

    std::uint16_t maxPixelStepQ12 = 0;
};

class RenderGainController {
public:
    explicit RenderGainController(
        RenderGainControllerConfig config = {})
        : config_(config) {}

    RenderGainContext update(
        const RenderGainContext& target,
        std::uint64_t nowUs);

    void reset();

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
        return current_.sameRenderProfileAs(target_);
    }

private:
    static std::uint16_t moveTowards(
        std::uint16_t current,
        std::uint16_t target,
        std::uint16_t maxStep);

    static void forceUnity(
        RenderGainContext& context);

    std::uint16_t maxStepForDeltaTime(
        std::uint64_t deltaUs) const;

    RenderGainControllerConfig config_{};

    RenderGainContext current_{};
    RenderGainContext target_{};

    bool initialized_ = false;
    std::uint64_t lastUpdateUs_ = 0;
    std::uint32_t lastTargetGeneration_ = 0;

    RenderGainControllerStats stats_{};
};

} // namespace ambilight
