#pragma once

#include <array>
#include <cstdint>

#include <Adafruit_VL53L5CX.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "tof/TofGainModel.h"
#include "tof/TofPerimeterGainModel.h"
#include "tof/TofPlaneChangeGate.h"
#include "tof/TofProcessor.h"
#include "tof/TofTypes.h"

namespace ambilight {

enum class TofState : std::uint8_t {
    NotStarted = 0,
    Initializing,
    Ranging,
    Error
};

struct TofSnapshot {
    TofState state = TofState::NotStarted;

    std::uint32_t generation = 0;
    std::uint64_t timestampUs = 0;

    std::array<std::int16_t, kTofZoneCount> distanceMm{};
    std::array<std::uint8_t, kTofZoneCount> targetStatus{};

    std::uint8_t validZones = 0;
    std::uint16_t medianMm = 0;

    TofGeometrySnapshot geometry{};
    GainSnapshot gains{};
    PerimeterGainSnapshot perimeterGains{};

    std::uint32_t initAttempts = 0;
    std::uint32_t initFailures = 0;
    std::uint32_t rangingReadFailures = 0;
    std::uint32_t staleRestarts = 0;
    std::uint32_t frames = 0;

    std::uint32_t planeRecalculations = 0;
    std::uint32_t planeDeadbandSkips = 0;
    std::uint32_t planeFailOpens = 0;
    std::uint16_t lastPlaneWallDeltaMm = 0;

    std::uint32_t gainCurveUpdates = 0;

    std::uint32_t lastReadUs = 0;
    std::uint32_t maxReadUs = 0;
};

class TofService {
public:
    TofService();
    ~TofService();

    TofService(const TofService&) = delete;
    TofService& operator=(const TofService&) = delete;

    bool begin();

    bool setGainCurve(
        const DistanceGainCurve& curve);

    bool copySnapshot(TofSnapshot& destination) const;
    bool copyGainSnapshot(GainSnapshot& destination) const;
    bool copyPerimeterGainSnapshot(
        PerimeterGainSnapshot& destination) const;

private:
    static void taskEntry(void* context);
    void taskLoop();

    bool initializeSensor();
    void publishState(TofState state);
    void publishResults(
        const VL53L5CX_ResultsData& results,
        std::uint32_t readUs,
        std::uint64_t timestampUs);
    void refreshGainStaleness(std::uint64_t nowUs);
    void applyPendingGainCurve();

    static bool isUsableStatus(std::uint8_t status);
    static std::uint16_t medianOfValid(
        const VL53L5CX_ResultsData& results,
        std::uint8_t& validCount);

    mutable SemaphoreHandle_t mutex_ = nullptr;
    TaskHandle_t task_ = nullptr;

    Adafruit_VL53L5CX* sensor_ = nullptr;
    VL53L5CX_ResultsData results_{};

    TofProcessor processor_;
    TofPlaneChangeGate planeChangeGate_;
    TofGainModel gainModel_;
    TofPerimeterGainModel perimeterGainModel_;

    DistanceGainCurve pendingGainCurve_{};
    bool pendingGainCurveDirty_ = false;

    TofSnapshot snapshot_{};
};

} // namespace ambilight
