#include "led/LedEngine.h"

#include <cstring>

#include <esp_timer.h>

namespace ambilight {

LedEngine::LedEngine()
    : group_(LED_STRIP_WS2812, config::kPhysicalLaneLength, false) {}

esp_err_t LedEngine::begin() {
    for (std::size_t lane = 0;
         lane < config::kParlioLaneCount;
         ++lane) {

        lanes_[lane] =
            &group_.addStrip(
                config::kLedGpios[lane]);

        if (lanes_[lane] == nullptr ||
            !lanes_[lane]->isValid()) {

            return ESP_ERR_INVALID_STATE;
        }
    }

    const esp_err_t result =
        group_.begin();

    if (result != ESP_OK) {
        return result;
    }

    group_.brightness(
        brightness_);

    for (std::size_t lane = 0;
         lane < config::kParlioLaneCount;
         ++lane) {

        const auto buffer =
            lanes_[lane]->
                pixelBufferView();

        if (buffer.data == nullptr ||
            buffer.pixel_count !=
                config::kPhysicalLaneLength ||
            buffer.bytes_per_pixel != 3 ||
            buffer.order != ORDER_GRB) {

            return ESP_ERR_INVALID_STATE;
        }

        frameWriteView_.lane[lane] = {
            buffer.data,
            static_cast<std::uint16_t>(
                buffer.pixel_count)
        };
    }

    if (!frameWriteView_.valid()) {
        return ESP_ERR_INVALID_STATE;
    }

    begun_ = true;

    clear();
    return show();
}

void LedEngine::setBrightness(
    std::uint8_t brightness) {

    brightness_ = brightness;

    if (begun_) {
        group_.brightness(
            brightness_);
    }
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
            frameWriteView_.lane.size() ||
        count == 0) {

        return false;
    }

    const auto& target =
        frameWriteView_.lane[lane];

    if (!target.valid() ||
        start >= target.pixelCount ||
        static_cast<std::uint32_t>(
            start) +
            count >
                target.pixelCount) {

        return false;
    }

    for (std::uint16_t offset = 0;
         offset < count;
         ++offset) {

        target.writeUnchecked(
            static_cast<std::uint16_t>(
                start +
                offset),
            color);
    }

    return true;
}

esp_err_t LedEngine::show() {
    const std::uint64_t showStartedUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    const std::uint64_t encodeStartedUs =
        showStartedUs;

    esp_err_t result =
        group_.encode();

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

    const std::uint64_t submitStartedUs =
        encodeFinishedUs;

    result =
        group_.transmit();

    const std::uint64_t submitFinishedUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    submitMetric_.observe(
        submitFinishedUs -
        submitStartedUs);

    if (result != ESP_OK) {
        showMetric_.observe(
            submitFinishedUs -
            showStartedUs);
        return result;
    }

    const std::uint64_t waitStartedUs =
        submitFinishedUs;

    result =
        group_.wait();

    const std::uint64_t waitFinishedUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    waitMetric_.observe(
        waitFinishedUs -
        waitStartedUs);

    showMetric_.observe(
        waitFinishedUs -
        showStartedUs);

    return result;
}

} // namespace ambilight
