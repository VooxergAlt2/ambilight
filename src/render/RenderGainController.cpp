#include "render/RenderGainController.h"

#include <algorithm>

namespace ambilight {

void RenderGainController::reset(
    const LedMappingProfile& topology) {

    current_ =
        RenderGainContext::unity(
            topology);

    target_ =
        RenderGainContext::unity(
            topology);

    initialized_ = false;
    settled_ = true;
    nonUnity_ = false;

    lastUpdateUs_ = 0;
    lastTargetGeneration_ = 0;

    stats_ = {};
}

std::uint16_t RenderGainController::moveTowards(
    std::uint16_t current,
    std::uint16_t target,
    std::uint16_t maxStep) {

    current =
        sanitizeGainQ12(current);

    target =
        sanitizeGainQ12(target);

    if (current == target ||
        maxStep == 0) {

        return current;
    }

    if (current < target) {
        const std::uint32_t next =
            static_cast<std::uint32_t>(
                current) +
            maxStep;

        return static_cast<std::uint16_t>(
            std::min<std::uint32_t>(
                target,
                next));
    }

    const std::uint16_t delta =
        static_cast<std::uint16_t>(
            current -
            target);

    return delta <= maxStep
        ? target
        : static_cast<std::uint16_t>(
              current -
              maxStep);
}

std::uint16_t
RenderGainController::maxStepForDeltaTime(
    std::uint64_t deltaUs) const {

    if (config_.slewQ12PerSecond == 0) {
        return kGainUnityQ12;
    }

    const std::uint64_t raw =
        (
            static_cast<std::uint64_t>(
                config_.slewQ12PerSecond) *
            deltaUs +
            999999ULL
        ) /
        1000000ULL;

    if (raw == 0 &&
        deltaUs > 0) {

        return 1;
    }

    return static_cast<std::uint16_t>(
        std::min<std::uint64_t>(
            kGainUnityQ12,
            raw));
}

void
RenderGainController::
    copyTargetMetadataToCurrent() {

    current_.sourceGeneration =
        target_.sourceGeneration;

    current_.sourceTimestampUs =
        target_.sourceTimestampUs;

    current_.sourceAgeUs =
        target_.sourceAgeUs;

    current_.sourcePresent =
        target_.sourcePresent;

    current_.sourceUsable =
        target_.sourceUsable;

    current_.failOpen =
        target_.failOpen;

    current_.topology =
        target_.topology;
}

bool RenderGainController::setTarget(
    const RenderGainContext& target,
    std::uint64_t nowUs) {

    ++stats_.targetUpdates;

    if (target.sourceUsable &&
        !target.failOpen) {

        ++stats_.usableTargets;
    } else {
        ++stats_.failOpenTargets;
    }

    if (target.sourceGeneration !=
        lastTargetGeneration_) {

        ++stats_.targetGenerationChanges;

        lastTargetGeneration_ =
            target.sourceGeneration;
    }

    const bool renderProfileChanged =
        !target_.
            sameRenderProfileAs(
                target);

    if (renderProfileChanged) {
        ++stats_.renderProfileChanges;
    }

    target_ =
        target;

    const bool failOpen =
        !target_.sourceUsable ||
        target_.failOpen;

    if (failOpen) {
        const bool wasAttenuated =
            nonUnity_;

        current_ =
            target_;

        current_.forceUnity();
        current_.sourceUsable = false;
        current_.failOpen = true;

        initialized_ = true;
        settled_ = true;
        nonUnity_ = false;
        lastUpdateUs_ = nowUs;

        if (wasAttenuated) {
            ++stats_.
                failOpenUnitySnaps;
        }

        return
            renderProfileChanged ||
            wasAttenuated;
    }

    if (!initialized_ ||
        !current_.sourceUsable ||
        current_.failOpen ||
        current_.topology.segment !=
            target_.topology.segment) {

        current_ =
            target_;

        current_.forceUnity();
        current_.sourceUsable = true;
        current_.failOpen = false;

        initialized_ = true;
        nonUnity_ = false;
        lastUpdateUs_ = nowUs;

        settled_ =
            current_.
                sameRenderProfileAs(
                    target_);

        return true;
    }

    copyTargetMetadataToCurrent();

    if (renderProfileChanged) {
        settled_ = false;
    }

    return renderProfileChanged;
}

bool RenderGainController::advance(
    std::uint64_t nowUs) {

    ++stats_.advances;

    if (!initialized_ ||
        settled_ ||
        !target_.sourceUsable ||
        target_.failOpen) {

        return false;
    }

    if (nowUs < lastUpdateUs_) {
        ++stats_.timeRollbacks;

        current_ =
            target_;

        current_.forceUnity();
        current_.sourceUsable = true;
        current_.failOpen = false;

        nonUnity_ = false;

        settled_ =
            current_.
                sameRenderProfileAs(
                    target_);

        lastUpdateUs_ =
            nowUs;

        return true;
    }

    const std::uint64_t deltaUs =
        nowUs -
        lastUpdateUs_;

    const std::uint16_t maxStep =
        maxStepForDeltaTime(
            deltaUs);

    stats_.maxPixelStepQ12 =
        std::max(
            stats_.maxPixelStepQ12,
            maxStep);

    bool changed = false;
    bool remaining = false;
    bool anyNonUnity = false;

    const std::size_t activeCount =
        target_.
            topology.
            totalLedCount();

    for (std::size_t index = 0;
         index < activeCount;
         ++index) {

        const std::uint16_t before =
            current_.
                logicalGainQ12[
                    index];

        const std::uint16_t targetGain =
            target_.
                logicalGainQ12[
                    index];

        const std::uint16_t after =
            moveTowards(
                before,
                targetGain,
                maxStep);

        current_.
            logicalGainQ12[
                index] =
            after;

        if (after != before) {
            changed = true;
        }

        if (after !=
            sanitizeGainQ12(
                targetGain)) {

            remaining = true;
        }

        if (sanitizeGainQ12(
                after) !=
            kGainUnityQ12) {

            anyNonUnity = true;
        }
    }

    copyTargetMetadataToCurrent();

    nonUnity_ =
        anyNonUnity;

    settled_ =
        !remaining;

    lastUpdateUs_ =
        nowUs;

    return changed;
}

} // namespace ambilight
