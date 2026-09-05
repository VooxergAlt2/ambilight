#include "render/RenderScheduler.h"

namespace ambilight {

void RenderScheduler::reset() {
    hasRendered_ = false;
    lastRenderStartedUs_ = 0;
    stats_ = {};
}

RenderDecision RenderScheduler::decide(
    bool hasRgbFrame,
    bool rgbDirty,
    bool stateDirty,
    std::uint64_t nowUs) {

    ++stats_.decisions;

    RenderDecision decision;

    if (!hasRgbFrame) {
        ++stats_.noFrameSkips;
        return decision;
    }

    if (rgbDirty) {
        decision.render = true;
        decision.dueToRgb = true;
        decision.dueToState = stateDirty;

        if (stateDirty) {
            ++stats_.combinedRenders;
        } else {
            ++stats_.rgbRenders;
        }

        return decision;
    }

    if (!stateDirty) {
        ++stats_.cleanSkips;
        return decision;
    }

    if (!hasRendered_) {
        decision.render = true;
        decision.dueToState = true;
        ++stats_.stateOnlyRenders;
        return decision;
    }

    if (nowUs < lastRenderStartedUs_) {
        ++stats_.timeRollbacks;

        decision.render = true;
        decision.dueToState = true;
        ++stats_.stateOnlyRenders;
        return decision;
    }

    const std::uint64_t elapsedUs =
        nowUs - lastRenderStartedUs_;

    if (elapsedUs <
        config_.stateOnlyMinIntervalUs) {

        decision.stateDeferred = true;
        ++stats_.stateDeferrals;
        return decision;
    }

    decision.render = true;
    decision.dueToState = true;
    ++stats_.stateOnlyRenders;

    return decision;
}

void RenderScheduler::markRendered(
    std::uint64_t renderStartedUs) {

    hasRendered_ = true;
    lastRenderStartedUs_ =
        renderStartedUs;
}

} // namespace ambilight
