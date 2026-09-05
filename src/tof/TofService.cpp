#include "tof/TofService.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

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
constexpr std::uint64_t kGainStaleTimeoutUs = 30000000ULL;
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

TofPlaneChangeGateConfig makePlaneChangeGateConfig() {
    TofPlaneChangeGateConfig gateConfig;
    gateConfig.geometry =
        config::kPerimeterScreenGeometry;
    gateConfig.wallDeltaDeadbandMm =
        config::kTofPlaneWallDeadbandMm;
    return gateConfig;
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
      planeChangeGate_(makePlaneChangeGateConfig()),
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
    planeChangeGate_.reset();

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

    TofSnapshot next;

    if (mutex_ != nullptr &&
        xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        next = snapshot_;
        xSemaphoreGive(mutex_);
    }

    TofPlaneChangeDecision planeDecision =
        planeChangeGate_.observe(
            geometry.plane);

    // If a previously accepted profile aged out into fail-open, a fresh
    // near-identical plane must restore it even though the geometry itself
    // remains inside the deadband.
    if (planeDecision.action ==
            TofPlaneChangeAction::RefreshOnly &&
        next.perimeterGains.failOpen) {

        planeDecision.action =
            TofPlaneChangeAction::Recalculate;
    }

    switch (planeDecision.action) {
    case TofPlaneChangeAction::Recalculate:
        next.gains =
            gainModel_.evaluate(
                geometry,
                timestampUs);

        next.perimeterGains =
            perimeterGainModel_.evaluate(
                geometry,
                timestampUs);

        ++next.planeRecalculations;
        break;

    case TofPlaneChangeAction::RefreshOnly:
        // The fresh sensor frame confirms that the last applied wall plane is
        // still current. Refresh source age/generation only. Do not rebuild
        // the 780-value distance/gain field.
        if (!next.gains.failOpen) {
            next.gains.generation =
                geometry.generation;
            next.gains.timestampUs =
                geometry.timestampUs;
        }

        if (!next.perimeterGains.failOpen) {
            next.perimeterGains.generation =
                geometry.generation;
            next.perimeterGains.timestampUs =
                geometry.timestampUs;
        }

        ++next.planeDeadbandSkips;
        break;

    case TofPlaneChangeAction::FailOpen:
        next.gains =
            gainModel_.evaluate(
                TofGeometrySnapshot{},
                timestampUs);

        next.perimeterGains =
            perimeterGainModel_.evaluate(
                TofGeometrySnapshot{},
                timestampUs);

        ++next.planeFailOpens;
        break;

    case TofPlaneChangeAction::None:
        break;
    }

    if (std::isfinite(
            planeDecision.maxWallDeltaMm)) {

        const float bounded =
            std::max(
                0.0F,
                std::min(
                    65535.0F,
                    planeDecision.maxWallDeltaMm));

        next.lastPlaneWallDeltaMm =
            static_cast<std::uint16_t>(
                std::lround(bounded));
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

    if (mutex_ == nullptr) {
        return;
    }

    GainSnapshot gains;
    PerimeterGainSnapshot perimeterGains;

    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        gains = snapshot_.gains;
        perimeterGains =
            snapshot_.perimeterGains;
        xSemaphoreGive(mutex_);
    }

    const auto stale =
        [nowUs](std::uint64_t timestampUs) {
            return
                timestampUs == 0 ||
                nowUs < timestampUs ||
                nowUs - timestampUs >
                    kGainStaleTimeoutUs;
        };

    const bool gainsStale =
        !gains.failOpen &&
        stale(gains.timestampUs);

    const bool perimeterStale =
        !perimeterGains.failOpen &&
        stale(perimeterGains.timestampUs);

    if (!gainsStale &&
        !perimeterStale) {
        return;
    }

    const TofGeometrySnapshot invalid{};

    if (gainsStale) {
        gains =
            gainModel_.evaluate(
                invalid,
                nowUs);
    }

    if (perimeterStale) {
        perimeterGains =
            perimeterGainModel_.evaluate(
                invalid,
                nowUs);
    }

    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        if (gainsStale) {
            snapshot_.gains = gains;
        }

        if (perimeterStale) {
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
