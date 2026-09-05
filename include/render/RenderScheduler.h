#pragma once

#include <cstdint>

namespace ambilight {

struct RenderSchedulerConfig {
    // Non-RGB state-only rerenders are rate-limited to about 60 Hz.
    // New RGB frames are never delayed by this limit.
    std::uint32_t stateOnlyMinIntervalUs = 16667;
};

struct RenderDecision {
    bool render = false;
    bool dueToRgb = false;
    bool dueToState = false;
    bool stateDeferred = false;
};

struct RenderSchedulerStats {
    std::uint32_t decisions = 0;

    std::uint32_t rgbRenders = 0;
    std::uint32_t stateOnlyRenders = 0;
    std::uint32_t combinedRenders = 0;

    std::uint32_t stateDeferrals = 0;
    std::uint32_t noFrameSkips = 0;
    std::uint32_t cleanSkips = 0;
    std::uint32_t timeRollbacks = 0;
};

class RenderScheduler {
public:
    explicit RenderScheduler(
        RenderSchedulerConfig config = {})
        : config_(config) {}

    RenderDecision decide(
        bool hasRgbFrame,
        bool rgbDirty,
        bool stateDirty,
        std::uint64_t nowUs);

    void markRendered(std::uint64_t renderStartedUs);

    void reset();

    const RenderSchedulerStats& stats() const {
        return stats_;
    }

    std::uint64_t lastRenderStartedUs() const {
        return lastRenderStartedUs_;
    }

private:
    RenderSchedulerConfig config_{};

    bool hasRendered_ = false;
    std::uint64_t lastRenderStartedUs_ = 0;

    RenderSchedulerStats stats_{};
};

} // namespace ambilight
