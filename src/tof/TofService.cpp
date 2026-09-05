#include "tof/TofService.h"

#include <algorithm>
#include <array>

#include <Arduino.h>
#include <Wire.h>
#include <esp_timer.h>

#include "config/BoardConfig.h"

namespace ambilight {
namespace {

constexpr std::uint8_t kResolution = 64;
constexpr std::uint8_t kRangingFrequencyHz = 10;

constexpr std::uint32_t kInitRetryMs = 5000;
constexpr std::uint32_t kPollDelayMs = 10;

// VL53L5CX supports Fast-mode Plus. With short local wiring this greatly
// reduces both firmware upload time and 8x8 result-read occupancy.
// If the actual breakout/wiring proves marginal, this is the first value to
// lower to 400000 during hardware acceptance.
constexpr std::uint32_t kI2cClockHz = 1000000;

constexpr std::uint32_t kTaskStackBytes = 12288;
constexpr UBaseType_t kTaskPriority = 0;

} // namespace

TofService::~TofService() {
    if (task_ != nullptr) {
        vTaskDelete(task_);
        task_ = nullptr;
    }

    if (sensor_ != nullptr) {
        delete sensor_;
        sensor_ = nullptr;
    }

    if (mutex_ != nullptr) {
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
    }
}

bool TofService::begin() {
    if (task_ != nullptr) {
        return true;
    }

    if (mutex_ == nullptr) {
        mutex_ = xSemaphoreCreateMutex();
        if (mutex_ == nullptr) {
            return false;
        }
    }

    const BaseType_t result = xTaskCreate(
        taskEntry,
        "ambilight-tof",
        kTaskStackBytes,
        this,
        kTaskPriority,
        &task_);

    if (result != pdPASS) {
        task_ = nullptr;
        return false;
    }

    return true;
}

bool TofService::copySnapshot(TofSnapshot& destination) const {
    if (mutex_ == nullptr) {
        return false;
    }

    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(5)) != pdTRUE) {
        return false;
    }

    destination = snapshot_;

    xSemaphoreGive(mutex_);
    return true;
}

void TofService::taskEntry(void* context) {
    auto* self = static_cast<TofService*>(context);
    self->taskLoop();

    self->task_ = nullptr;
    vTaskDelete(nullptr);
}

void TofService::publishState(TofState state) {
    if (mutex_ == nullptr) {
        return;
    }

    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        snapshot_.state = state;
        xSemaphoreGive(mutex_);
    }
}

bool TofService::initializeSensor() {
    if (mutex_ != nullptr &&
        xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        ++snapshot_.initAttempts;
        snapshot_.state = TofState::Initializing;
        xSemaphoreGive(mutex_);
    }

    if (sensor_ != nullptr) {
        delete sensor_;
        sensor_ = nullptr;
    }

    sensor_ = new Adafruit_VL53L5CX();
    if (sensor_ == nullptr) {
        return false;
    }

    Wire.end();

    if (!Wire.begin(
            config::kTofSdaGpio,
            config::kTofSclGpio,
            kI2cClockHz)) {
        return false;
    }

    if (!sensor_->begin(
            VL53L5CX_DEFAULT_ADDRESS,
            &Wire,
            kI2cClockHz)) {
        return false;
    }

    if (!sensor_->setResolution(kResolution)) {
        return false;
    }

    if (!sensor_->setRangingFrequency(kRangingFrequencyHz)) {
        return false;
    }

    if (!sensor_->startRanging()) {
        return false;
    }

    publishState(TofState::Ranging);
    return true;
}

bool TofService::isUsableStatus(std::uint8_t status) {
    // ST ULD documentation marks 5 and 9 as valid ranging results.
    return status == 5 || status == 9;
}

std::uint16_t TofService::medianOfValid(
    const VL53L5CX_ResultsData& results,
    std::uint8_t& validCount) {

    std::array<std::uint16_t, 64> valid{};
    std::size_t count = 0;

    for (std::size_t index = 0; index < 64; ++index) {
        const std::int16_t distance = results.distance_mm[index];
        const std::uint8_t status = results.target_status[index];

        if (!isUsableStatus(status) || distance <= 0) {
            continue;
        }

        valid[count++] = static_cast<std::uint16_t>(distance);
    }

    validCount = static_cast<std::uint8_t>(count);

    if (count == 0) {
        return 0;
    }

    std::sort(valid.begin(), valid.begin() + count);

    if ((count & 1U) != 0U) {
        return valid[count / 2];
    }

    const std::uint32_t a = valid[count / 2 - 1];
    const std::uint32_t b = valid[count / 2];

    return static_cast<std::uint16_t>((a + b) / 2U);
}

void TofService::publishResults(
    const VL53L5CX_ResultsData& results,
    std::uint32_t readUs,
    std::uint64_t timestampUs) {

    TofSnapshot next;

    if (mutex_ != nullptr &&
        xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        next = snapshot_;
        xSemaphoreGive(mutex_);
    }

    next.state = TofState::Ranging;
    next.timestampUs = timestampUs;
    ++next.generation;
    ++next.frames;

    next.lastReadUs = readUs;
    if (readUs > next.maxReadUs) {
        next.maxReadUs = readUs;
    }

    for (std::size_t index = 0; index < 64; ++index) {
        next.distanceMm[index] = results.distance_mm[index];
        next.targetStatus[index] = results.target_status[index];
    }

    next.medianMm = medianOfValid(
        results,
        next.validZones);

    if (mutex_ != nullptr &&
        xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        snapshot_ = next;
        xSemaphoreGive(mutex_);
    }
}

void TofService::taskLoop() {
    for (;;) {
        if (!initializeSensor()) {
            if (mutex_ != nullptr &&
                xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
                ++snapshot_.initFailures;
                snapshot_.state = TofState::Error;
                xSemaphoreGive(mutex_);
            }

            vTaskDelay(pdMS_TO_TICKS(kInitRetryMs));
            continue;
        }

        for (;;) {
            if (!sensor_->isDataReady()) {
                vTaskDelay(pdMS_TO_TICKS(kPollDelayMs));
                continue;
            }

            const std::uint64_t readStartedUs =
                static_cast<std::uint64_t>(esp_timer_get_time());

            if (!sensor_->getRangingData(&results_)) {
                if (mutex_ != nullptr &&
                    xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
                    ++snapshot_.rangingReadFailures;
                    xSemaphoreGive(mutex_);
                }

                vTaskDelay(pdMS_TO_TICKS(kPollDelayMs));
                continue;
            }

            const std::uint64_t readFinishedUs =
                static_cast<std::uint64_t>(esp_timer_get_time());

            publishResults(
                results_,
                static_cast<std::uint32_t>(
                    readFinishedUs - readStartedUs),
                readFinishedUs);

            vTaskDelay(pdMS_TO_TICKS(kPollDelayMs));
        }
    }
}

} // namespace ambilight
