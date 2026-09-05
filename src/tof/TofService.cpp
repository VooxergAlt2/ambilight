#include "tof/TofService.h"

#include <algorithm>
#include <array>

#include <Arduino.h>
#include <Wire.h>
#include <esp_timer.h>

#include "config/BoardConfig.h"
#include "config/ScreenGeometry.h"
#include "config/TofCalibration.h"

namespace ambilight {
namespace {

constexpr std::uint8_t kResolution = 64;
constexpr std::uint8_t kRangingFrequencyHz = 1;

constexpr std::uint32_t kInitRetryMs = 5000;
constexpr std::uint32_t kPollDelayMs = 100;
constexpr std::uint32_t kRangingStaleMs = 30000;
constexpr std::uint32_t kMeasurementIntervalMs = 12000;
constexpr std::uint8_t kMaxConsecutiveReadFailures = 5;

constexpr std::uint32_t kI2cClockHz = 1000000;

constexpr std::uint32_t kTaskStackBytes = 12288;
constexpr UBaseType_t kTaskPriority = 0;

TofProcessorConfig makeProcessorConfig() {
    TofProcessorConfig processorConfig;

    switch (config::kTofRotationQuarterTurns % 4U) {
    case 1:
        processorConfig.transform.rotation = TofRotation::Deg90;
        break;
    case 2:
        processorConfig.transform.rotation = TofRotation::Deg180;
        break;
    case 3:
        processorConfig.transform.rotation = TofRotation::Deg270;
        break;
    default:
        processorConfig.transform.rotation = TofRotation::Deg0;
        break;
    }

    processorConfig.transform.mirrorX = config::kTofMirrorX;

    return processorConfig;
}

TofGainModelConfig makeGainModelConfig() {
    TofGainModelConfig gainConfig;

    gainConfig.curve.configure(
        config::kTofGainPoints,
        config::kTofGainPointCount);

    return gainConfig;
}

TofPerimeterGainModelConfig makePerimeterGainModelConfig() {
    TofPerimeterGainModelConfig gainConfig;

    gainConfig.curve.configure(
        config::kTofGainPoints,
        config::kTofGainPointCount);

    gainConfig.geometry =
        config::kPerimeterScreenGeometry;

    return gainConfig;
}

} // namespace

TofService::TofService()
    : processor_(makeProcessorConfig()),
      gainModel_(makeGainModelConfig()),
      perimeterGainModel_(
          makePerimeterGainModelConfig()) {}

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

bool TofService::copyGainSnapshot(
    GainSnapshot& destination) const {

    if (mutex_ == nullptr) {
        return false;
    }

    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(2)) != pdTRUE) {
        return false;
    }

    destination = snapshot_.gains;

    xSemaphoreGive(mutex_);
    return true;
}

bool TofService::copyPerimeterGainSnapshot(
    PerimeterGainSnapshot& destination) const {

    if (mutex_ == nullptr) {
        return false;
    }

    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(2)) != pdTRUE) {
        return false;
    }

    destination =
        snapshot_.perimeterGains;

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

    processor_.reset();

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
    return status == 5 || status == 6 || status == 9;
}

std::uint16_t TofService::medianOfValid(
    const VL53L5CX_ResultsData& results,
    std::uint8_t& validCount) {

    std::array<std::uint16_t, kTofZoneCount> valid{};
    std::size_t count = 0;

    for (std::size_t index = 0; index < kTofZoneCount; ++index) {
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

    TofRawFrame raw;
    raw.timestampUs = timestampUs;

    for (std::size_t index = 0; index < kTofZoneCount; ++index) {
        raw.distanceMm[index] = results.distance_mm[index];
        raw.targetStatus[index] = results.target_status[index];
    }

    const TofGeometrySnapshot geometry =
        processor_.process(raw);

    const GainSnapshot gains =
        gainModel_.evaluate(
            geometry,
            timestampUs);

    const PerimeterGainSnapshot perimeterGains =
        perimeterGainModel_.evaluate(
            geometry,
            timestampUs);

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

    next.distanceMm = raw.distanceMm;
    next.targetStatus = raw.targetStatus;
    next.geometry = geometry;
    next.gains = gains;
    next.perimeterGains =
        perimeterGains;

    next.medianMm = medianOfValid(
        results,
        next.validZones);

    if (mutex_ != nullptr &&
        xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        snapshot_ = next;
        xSemaphoreGive(mutex_);
    }
}

void TofService::refreshGainStaleness(
    std::uint64_t nowUs) {

    TofGeometrySnapshot geometry;

    if (mutex_ == nullptr) {
        return;
    }

    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        geometry = snapshot_.geometry;
        xSemaphoreGive(mutex_);
    }

    const GainSnapshot gains =
        gainModel_.evaluate(
            geometry,
            nowUs);

    const PerimeterGainSnapshot perimeterGains =
        perimeterGainModel_.evaluate(
            geometry,
            nowUs);

    if (!gains.failOpen &&
        !perimeterGains.failOpen) {
        return;
    }

    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        if (gains.failOpen) {
            snapshot_.gains = gains;
        }

        if (perimeterGains.failOpen) {
            snapshot_.perimeterGains =
                perimeterGains;
        }

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
                const auto nowUs =
                    static_cast<std::uint64_t>(
                        esp_timer_get_time());

                snapshot_.gains =
                    gainModel_.evaluate(
                        TofGeometrySnapshot{},
                        nowUs);

                snapshot_.perimeterGains =
                    perimeterGainModel_.evaluate(
                        TofGeometrySnapshot{},
                        nowUs);

                xSemaphoreGive(mutex_);
            }

            vTaskDelay(pdMS_TO_TICKS(kInitRetryMs));
            continue;
        }

        std::uint64_t lastSuccessfulFrameUs =
            static_cast<std::uint64_t>(esp_timer_get_time());

        std::uint8_t consecutiveReadFailures = 0;

        for (;;) {
            const std::uint64_t nowUs =
                static_cast<std::uint64_t>(esp_timer_get_time());

            if (!sensor_->isDataReady()) {
                refreshGainStaleness(nowUs);

                if (nowUs - lastSuccessfulFrameUs >
                    static_cast<std::uint64_t>(
                        kRangingStaleMs) * 1000ULL) {

                    if (mutex_ != nullptr &&
                        xSemaphoreTake(
                            mutex_,
                            portMAX_DELAY) == pdTRUE) {
                        ++snapshot_.staleRestarts;
                        snapshot_.state = TofState::Error;
                        xSemaphoreGive(mutex_);
                    }

                    break;
                }

                vTaskDelay(pdMS_TO_TICKS(kPollDelayMs));
                continue;
            }

            const std::uint64_t readStartedUs =
                static_cast<std::uint64_t>(esp_timer_get_time());

            if (!sensor_->getRangingData(&results_)) {
                if (mutex_ != nullptr &&
                    xSemaphoreTake(
                        mutex_,
                        portMAX_DELAY) == pdTRUE) {
                    ++snapshot_.rangingReadFailures;
                    xSemaphoreGive(mutex_);
                }

                ++consecutiveReadFailures;
                refreshGainStaleness(nowUs);

                if (consecutiveReadFailures >=
                    kMaxConsecutiveReadFailures) {

                    publishState(TofState::Error);
                    break;
                }

                vTaskDelay(pdMS_TO_TICKS(kPollDelayMs));
                continue;
            }

            const std::uint64_t readFinishedUs =
                static_cast<std::uint64_t>(esp_timer_get_time());

            lastSuccessfulFrameUs = readFinishedUs;
            consecutiveReadFailures = 0;

            publishResults(
                results_,
                static_cast<std::uint32_t>(
                    readFinishedUs - readStartedUs),
                readFinishedUs);

            // TV pose changes slowly. Keep the sensor initialized and ranging
            // internally at 1 Hz, but only transfer/process one 8x8 frame
            // every ~12 seconds.
            vTaskDelay(pdMS_TO_TICKS(kMeasurementIntervalMs));
        }

        vTaskDelay(pdMS_TO_TICKS(kInitRetryMs));
    }
}

} // namespace ambilight
