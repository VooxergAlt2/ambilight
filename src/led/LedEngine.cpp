#include "led/LedEngine.h"

#include <cstring>

#include <esp_timer.h>

namespace ambilight {

LedEngine::LedEngine() = default;

esp_err_t LedEngine::initializeGroup(
    std::size_t physicalLaneLength) {

    if (physicalLaneLength == 0 ||
        physicalLaneLength >
            config::kMaxRepresentablePhysicalLaneLength ||
        group_.has_value()) {

        return ESP_ERR_INVALID_ARG;
    }

    group_.emplace(
        LED_STRIP_WS2812,
        physicalLaneLength,
        false);

    lanes_.fill(nullptr);
    frameWriteView_ = {};

    for (std::size_t lane = 0;
         lane < config::kParlioLaneCount;
         ++lane) {

        lanes_[lane] =
            &group_->addStrip(
                config::kLedGpios[lane]);

        if (lanes_[lane] == nullptr ||
            !lanes_[lane]->isValid()) {

            releaseGroup();
            return ESP_ERR_INVALID_STATE;
        }
    }

    const esp_err_t result =
        group_->begin();

    if (result != ESP_OK) {
        releaseGroup();
        return result;
    }

    group_->brightness(
        brightness_);

    for (std::size_t lane = 0;
         lane < config::kParlioLaneCount;
         ++lane) {

        const auto buffer =
            lanes_[lane]->
                pixelBufferView();

        if (buffer.data == nullptr ||
            buffer.pixel_count !=
                physicalLaneLength ||
            buffer.bytes_per_pixel != 3 ||
            buffer.order != ORDER_GRB ||
            buffer.pixel_count > 0xFFFFU) {

            releaseGroup();
            return ESP_ERR_INVALID_STATE;
        }

        frameWriteView_.lane[lane] = {
            buffer.data,
            static_cast<std::uint16_t>(
                buffer.pixel_count)
        };
    }

    if (!frameWriteView_.valid()) {
        releaseGroup();
        return ESP_ERR_INVALID_STATE;
    }

    begun_ = true;
    physicalLaneLength_ =
        physicalLaneLength;

    clear();

    // The startup/reconfiguration blackout must not be rejected merely
    // because a physical hole from the previous topology lies beyond a newly-shortened
    // lane. Preserve the configured mask state, but emit this one black frame with an
    // empty physical mask. The renderer reprojects the mask immediately after
    // a successful topology transaction.
    const LedPhysicalPixelMask savedMask =
        physicalPixelMask_;

    physicalPixelMask_ = {};

    const esp_err_t showResult =
        show();

    esp_err_t completionResult =
        showResult;

    if (showResult == ESP_OK) {
        completionResult =
            waitForIdle();
    }

    physicalPixelMask_ =
        savedMask;

    if (completionResult != ESP_OK) {
        releaseGroup();
        return completionResult;
    }

    return ESP_OK;
}

void LedEngine::releaseGroup() {
    begun_ = false;
    physicalLaneLength_ = 0;
    frameWriteView_ = {};
    lanes_.fill(nullptr);
    group_.reset();
}

esp_err_t LedEngine::begin(
    std::size_t physicalLaneLength) {

    if (begun_ ||
        group_.has_value()) {

        return ESP_ERR_INVALID_STATE;
    }

    return
        initializeGroup(
            physicalLaneLength);
}

esp_err_t LedEngine::reconfigurePhysicalLaneLength(
    std::size_t physicalLaneLength) {

    if (physicalLaneLength == 0 ||
        physicalLaneLength >
            config::kMaxRepresentablePhysicalLaneLength) {

        return ESP_ERR_INVALID_ARG;
    }

    if (!begun_) {
        return begin(
            physicalLaneLength);
    }

    if (physicalLaneLength ==
        physicalLaneLength_) {

        return ESP_OK;
    }

    const esp_err_t idleResult =
        waitForIdle();

    if (idleResult != ESP_OK) {
        return idleResult;
    }

    const std::size_t previousLength =
        physicalLaneLength_;

    releaseGroup();

    const esp_err_t requestedResult =
        initializeGroup(
            physicalLaneLength);

    if (requestedResult == ESP_OK) {
        return ESP_OK;
    }

    // Recreate the previous known-good geometry before returning a rejected
    // topology change. If this rare rollback also fails, the caller can detect
    // physicalLaneLength()==0 and must keep output black until reboot.
    const esp_err_t rollbackResult =
        initializeGroup(
            previousLength);

    if (rollbackResult != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }

    return requestedResult;
}

void LedEngine::setBrightness(
    std::uint8_t brightness) {

    brightness_ = brightness;

    if (begun_ &&
        group_) {

        group_->brightness(
            brightness_);
    }
}

bool LedEngine::setPhysicalPixelMask(
    const LedPhysicalPixelMask& mask) {

    if (!mask.valid()) {
        return false;
    }

    physicalPixelMask_ =
        mask;

    return true;
}

void LedEngine::clear() {
    if (frameWriteView_.valid()) {
        for (const auto& lane :
             frameWriteView_.lane) {

            std::memset(
                lane.grb,
                0,
                static_cast<std::size_t>(
                    lane.pixelCount) *
                    3U);
        }

        return;
    }

    // Before begin() there is no allocated pixel buffer yet. Preserve the
    // historical no-op behavior for early topology setup.
    for (auto* lane :
         lanes_) {

        if (lane != nullptr) {
            lane->clear();
        }
    }
}

bool LedEngine::fillPhysicalRange(
    std::uint8_t lane,
    std::uint16_t start,
    std::uint16_t count,
    const Rgb8& color) {

    if (lane >=
        frameWriteView_.lane.size()) {

        return false;
    }

    return
        frameWriteView_.
            lane[lane].
                fill(
                    start,
                    count,
                    color);
}

esp_err_t LedEngine::show() {
    if (!begun_ ||
        !group_) {

        return ESP_ERR_INVALID_STATE;
    }

    // Disabled pixels are a physical output invariant. Enforce them at the
    // lowest common path so DDP, ACTIVE correction and raw commissioning can
    // never re-light a masked LED.
    if (!physicalPixelMask_.apply(
            frameWriteView_)) {

        return ESP_ERR_INVALID_STATE;
    }

    const std::uint64_t showStartedUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    const bool hadInFlight =
        group_->inFlight();

    if (hadInFlight) {
        ++overlappedShows_;
    } else {
        ++coldShows_;
    }

    const std::uint64_t encodeStartedUs =
        showStartedUs;

    esp_err_t result =
        group_->encode();

    const std::uint64_t encodeFinishedUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    encodeMetric_.observe(
        encodeFinishedUs -
        encodeStartedUs);

    if (result != ESP_OK) {
        showMetric_.observe(
            encodeFinishedUs -
            showStartedUs);

        return result;
    }

    if (hadInFlight) {
        const std::uint64_t waitStartedUs =
            static_cast<std::uint64_t>(
                esp_timer_get_time());

        result =
            group_->wait();

        const std::uint64_t waitFinishedUs =
            static_cast<std::uint64_t>(
                esp_timer_get_time());

        waitMetric_.observe(
            waitFinishedUs -
            waitStartedUs);

        if (result != ESP_OK) {
            showMetric_.observe(
                waitFinishedUs -
                showStartedUs);

            return result;
        }

        ++completedFrames_;
    }

    const std::uint64_t submitStartedUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    result =
        group_->transmit();

    const std::uint64_t submitFinishedUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    submitMetric_.observe(
        submitFinishedUs -
        submitStartedUs);

    showMetric_.observe(
        submitFinishedUs -
        showStartedUs);

    if (result == ESP_OK) {
        ++submittedFrames_;
    }

    return result;
}

esp_err_t LedEngine::waitForIdle() {
    if (!begun_ ||
        !group_ ||
        !group_->inFlight()) {

        return ESP_OK;
    }

    const std::uint64_t waitStartedUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    const esp_err_t result =
        group_->wait();

    const std::uint64_t waitFinishedUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    flushWaitMetric_.observe(
        waitFinishedUs -
        waitStartedUs);

    if (result == ESP_OK) {
        ++completedFrames_;
    }

    return result;
}

} // namespace ambilight
