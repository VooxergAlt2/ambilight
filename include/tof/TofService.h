#pragma once

#include <array>
#include <cstdint>

#include <Adafruit_VL53L5CX.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

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

    std::array<std::int16_t, 64> distanceMm{};
    std::array<std::uint8_t, 64> targetStatus{};

    std::uint8_t validZones = 0;
    std::uint16_t medianMm = 0;

    std::uint32_t initAttempts = 0;
    std::uint32_t initFailures = 0;
    std::uint32_t rangingReadFailures = 0;
    std::uint32_t frames = 0;

    std::uint32_t lastReadUs = 0;
    std::uint32_t maxReadUs = 0;
};

class TofService {
public:
    TofService() = default;
    ~TofService();

    TofService(const TofService&) = delete;
    TofService& operator=(const TofService&) = delete;

    bool begin();

    bool copySnapshot(TofSnapshot& destination) const;

private:
    static void taskEntry(void* context);
    void taskLoop();

    bool initializeSensor();
    void publishState(TofState state);
    void publishResults(
        const VL53L5CX_ResultsData& results,
        std::uint32_t readUs,
        std::uint64_t timestampUs);

    static bool isUsableStatus(std::uint8_t status);
    static std::uint16_t medianOfValid(
        const VL53L5CX_ResultsData& results,
        std::uint8_t& validCount);

    mutable SemaphoreHandle_t mutex_ = nullptr;
    TaskHandle_t task_ = nullptr;

    Adafruit_VL53L5CX* sensor_ = nullptr;
    VL53L5CX_ResultsData results_{};

    TofSnapshot snapshot_{};
};

} // namespace ambilight
