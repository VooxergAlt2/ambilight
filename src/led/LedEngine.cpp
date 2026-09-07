#include "led/LedEngine.h"

#include <esp_timer.h>

namespace ambilight {

LedEngine::LedEngine()
    : group_(LED_STRIP_WS2812, config::kPhysicalLaneLength, false) {}

esp_err_t LedEngine::begin() {
    for (std::size_t lane = 0; lane < config::kParlioLaneCount; ++lane) {
        lanes_[lane] = &group_.addStrip(config::kLedGpios[lane]);

        if (lanes_[lane] == nullptr || !lanes_[lane]->isValid()) {
            return ESP_ERR_INVALID_STATE;
        }
    }

    const esp_err_t result = group_.begin();
    if (result != ESP_OK) {
        return result;
    }

    begun_ = true;
    group_.brightness(brightness_);

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
    for (auto* lane : lanes_) {
        if (lane != nullptr) {
            lane->clear();
        }
    }
}

bool LedEngine::setPhysicalPixel(
    std::uint8_t lane,
    std::uint16_t physicalIndex,
    crgb_t color) {

    if (lane >= lanes_.size() ||
        physicalIndex >= config::kPhysicalLaneLength ||
        lanes_[lane] == nullptr) {
        return false;
    }

    return lanes_[lane]->setPixel(physicalIndex, color) == ESP_OK;
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
