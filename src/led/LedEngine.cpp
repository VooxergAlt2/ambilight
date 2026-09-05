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

    group_.brightness(config::kTestBrightness);

    clear();
    return show();
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
    const std::int64_t startedUs = esp_timer_get_time();
    const esp_err_t result = group_.show();
    const std::int64_t finishedUs = esp_timer_get_time();

    const auto elapsed = static_cast<std::uint32_t>(finishedUs - startedUs);
    lastShowTimeUs_ = elapsed;
    if (elapsed > maxShowTimeUs_) {
        maxShowTimeUs_ = elapsed;
    }

    return result;
}

} // namespace ambilight
