#include "render/RenderGainController.h"

#include <algorithm>

namespace ambilight {

void RenderGainController::reset() {
    current_ = RenderGainContext::unity();
    target_ = RenderGainContext::unity();

    initialized_ = false;
    lastUpdateUs_ = 0;
    lastTargetGeneration_ = 0;

    stats_ = {};
}

std::uint16_t RenderGainController::moveTowards(
    std::uint16_t current,
    std::uint16_t target,
    std::uint16_t maxStep) {

    current = sanitizeGainQ12(current);
    target = sanitizeGainQ12(target);

    if (current == target || maxStep == 0) {
        return current;
    }

    if (current < target) {
        const std::uint32_t next =
            static_cast<std::uint32_t>(current) +
            maxStep;

        return static_cast<std::uint16_t>(
            std::min<std::uint32_t>(
                target,
                next));
    }

    const std::uint16_t delta =
        static_cast<std::uint16_t>(
            current - target);

    return delta <= maxStep
        ? target
        : static_cast<std::uint16_t>(
              current - maxStep);
}

void RenderGainController::forceUnity(
    RenderGainContext& context) {

    for (auto& endpoints : context.segmentGain) {
        endpoints.startQ12 = kGainUnityQ12;
        endpoints.endQ12 = kGainUnityQ12;
    }
}

std::uint16_t RenderGainController::maxStepForDeltaTime(
    std::uint64_t deltaUs) const {

    if (config_.slewQ12PerSecond == 0) {
        return kGainUnityQ12;
    }

    const std::uint64_t raw =
        (static_cast<std::uint64_t>(
             config_.slewQ12PerSecond) *
         deltaUs +
         999999ULL) /
        1000000ULL;

    if (raw == 0 && deltaUs > 0) {
        return 1;
    }

    return static_cast<std::uint16_t>(
        std::min<std::uint64_t>(
            kGainUnityQ12,
            raw));
}

RenderGainContext RenderGainController::update(
    const RenderGainContext& target,
    std::uint64_t nowUs) {

    ++stats_.updates;
    target_ = target;

    if (target.sourceUsable && !target.failOpen) {
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

    const bool failOpen =
        !target.sourceUsable ||
        target.failOpen;

    if (failOpen) {
        const bool wasAttenuated =
            current_.hasNonUnityGain();

        current_ = target;
        forceUnity(current_);

        current_.sourceUsable = false;
        current_.failOpen = true;

        if (initialized_ && wasAttenuated) {
            ++stats_.failOpenUnitySnaps;
        }

        initialized_ = true;
        lastUpdateUs_ = nowUs;

        return current_;
    }

    if (!initialized_ ||
        !current_.sourceUsable ||
        current_.failOpen) {

        current_ = target;
        forceUnity(current_);

        current_.sourcePresent =
            target.sourcePresent;
        current_.sourceUsable = true;
        current_.failOpen = false;

        initialized_ = true;
        lastUpdateUs_ = nowUs;

        return current_;
    }

    if (nowUs < lastUpdateUs_) {
        ++stats_.timeRollbacks;

        current_ = target;
        forceUnity(current_);

        current_.sourceUsable = true;
        current_.failOpen = false;

        lastUpdateUs_ = nowUs;
        return current_;
    }

    const std::uint64_t deltaUs =
        nowUs - lastUpdateUs_;

    const std::uint16_t maxStep =
        maxStepForDeltaTime(deltaUs);

    stats_.maxEndpointStepQ12 =
        std::max(
            stats_.maxEndpointStepQ12,
            maxStep);

    for (std::size_t index = 0;
         index < current_.segmentGain.size();
         ++index) {

        current_.segmentGain[index].startQ12 =
            moveTowards(
                current_.segmentGain[index].startQ12,
                target.segmentGain[index].startQ12,
                maxStep);

        current_.segmentGain[index].endQ12 =
            moveTowards(
                current_.segmentGain[index].endQ12,
                target.segmentGain[index].endQ12,
                maxStep);
    }

    current_.sourceGeneration =
        target.sourceGeneration;
    current_.sourceTimestampUs =
        target.sourceTimestampUs;
    current_.sourceAgeUs =
        target.sourceAgeUs;
    current_.sourcePresent =
        target.sourcePresent;
    current_.sourceUsable = true;
    current_.failOpen = false;

    lastUpdateUs_ = nowUs;

    return current_;
}

} // namespace ambilight
