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
    bool gainDirty,
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
        decision.dueToGain = gainDirty;

        if (gainDirty) {
            ++stats_.combinedRenders;
        } else {
            ++stats_.rgbRenders;
        }

        return decision;
    }

    if (!gainDirty) {
        ++stats_.cleanSkips;
        return decision;
    }

    if (!hasRendered_) {
        decision.render = true;
        decision.dueToGain = true;
        ++stats_.gainOnlyRenders;
        return decision;
    }

    if (nowUs < lastRenderStartedUs_) {
        ++stats_.timeRollbacks;

        decision.render = true;
        decision.dueToGain = true;
        ++stats_.gainOnlyRenders;
        return decision;
    }

    const std::uint64_t elapsedUs =
        nowUs - lastRenderStartedUs_;

    if (elapsedUs <
        config_.gainOnlyMinIntervalUs) {

        decision.gainDeferred = true;
        ++stats_.gainDeferrals;
        return decision;
    }

    decision.render = true;
    decision.dueToGain = true;
    ++stats_.gainOnlyRenders;

    return decision;
}

void RenderScheduler::markRendered(
    std::uint64_t renderStartedUs) {

    hasRendered_ = true;
    lastRenderStartedUs_ =
        renderStartedUs;
}

} // namespace ambilight
