#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "led/LedMappingProfile.h"
#include "led/LedPixelMaskProfile.h"
#include "network/WebUiProtocol.h"
#include "render/CorrectionMode.h"
#include "tof/TofCalibrationCapture.h"
#include "tof/TofGainModel.h"
#include "tof/TofSpatialProfile.h"
#include "tof/TofTypes.h"

namespace ambilight {

struct WebUiActionEvent {
    static constexpr std::size_t kPayloadCapacity =
        WebUiProtocol::kMaxBodyBytes + 1;

    std::uint32_t sequence = 0;
    WebUiActionKind kind =
        WebUiActionKind::None;

    std::array<
        char,
        kPayloadCapacity>
        payload{};

    bool ready() const {
        return
            kind !=
                WebUiActionKind::None &&
            sequence != 0;
    }

    const char* text() const {
        return payload.data();
    }

    char* text() {
        return payload.data();
    }
};

struct WebUiSnapshot {
    bool persistenceAvailable = false;

    CorrectionMode correctionMode =
        CorrectionMode::Shadow;

    std::uint8_t brightness = 0;
    bool outputIdleBlanked = false;

    bool wifiEnabled = false;
    bool wifiConnected = false;
    std::array<char, 33> wifiSsid{};
    std::array<char, 16> wifiIp{};
    std::int32_t wifiRssi = 0;

    bool ddpRunning = false;
    bool ddpHasFrame = false;
    std::uint64_t ddpFrameAgeMs = 0;
    std::uint32_t ddpCompleteFrames = 0;
    std::uint32_t ddpPublications = 0;

    bool senderLocked = false;
    std::array<char, 16> senderIp{};
    std::uint16_t senderPort = 0;

    bool tofAvailable = false;
    std::array<char, 16> tofState{};
    std::uint32_t tofGeneration = 0;
    std::uint64_t tofAgeMs = 0;
    std::uint8_t tofValidZones = 0;
    std::uint16_t tofMedianMm = 0;

    bool planeValid = false;
    std::int16_t planeYawCentiDeg = 0;
    std::int16_t planePitchCentiDeg = 0;
    std::uint8_t planeAccepted = 0;

    bool perimeterFailOpen = true;
    std::uint16_t perimeterMinMm = 0;
    std::uint16_t perimeterMaxMm = 0;

    bool tofDebugActive = false;
    std::uint32_t tofDebugRemainingMs = 0;

    std::array<
        std::int16_t,
        kTofZoneCount>
        tofNormalizedDistanceMm{};

    std::array<
        std::uint8_t,
        kTofZoneCount>
        tofNormalizedStatus{};

    std::array<
        std::uint8_t,
        kTofZoneCount>
        tofNormalizedRawIndex{};

    TofSpatialProfile spatialProfile{};
    bool spatialCustomized = false;
    bool spatialPersisted = false;

    std::array<
        GainPoint,
        DistanceGainCurve::kMaxPoints>
        gainPoints{};

    std::size_t gainPointCount = 0;
    bool gainCustomized = false;
    bool gainPersisted = false;

    LedMappingProfile ledMapping{};
    bool ledMappingCustomized = false;
    bool ledMappingPersisted = false;

    LedPixelMaskProfile ledPixelMask{};
    bool ledPixelMaskCustomized = false;
    bool ledPixelMaskPersisted = false;

    std::uint8_t commissioningPattern = 0;
    std::uint32_t commissioningRemainingMs = 0;
    std::uint8_t commissioningMaxBrightness = 0;
    std::uint8_t commissioningSide = 0;
    std::uint8_t commissioningGpio = 0;
    std::uint16_t commissioningRangeStart = 0;
    std::uint16_t commissioningRangeCount = 0;

    bool calibrationActive = false;
    std::uint32_t calibrationSamples = 0;
    bool calibrationSummaryAvailable = false;
    CalibrationCaptureSummary calibrationSummary{};

    bool shadowProbeActive = false;

    std::uint32_t lastActionSequence = 0;
    bool lastActionOk = true;
    std::array<char, 96> lastActionMessage{};

    std::uint32_t freeHeapBytes = 0;
    std::uint32_t minFreeHeapBytes = 0;
};

using WebUiSnapshotProvider =
    bool (*)(WebUiSnapshot& snapshot);

struct WebUiStats {
    std::uint32_t starts = 0;
    std::uint32_t startFailures = 0;

    std::uint32_t connections = 0;
    std::uint32_t requests = 0;
    std::uint32_t actionsQueued = 0;
    std::uint32_t actionsDropped = 0;

    std::uint32_t badRequests = 0;
    std::uint32_t forbiddenRequests = 0;
    std::uint32_t notFoundRequests = 0;

    std::uint32_t receiveErrors = 0;
    std::uint32_t sendErrors = 0;
    std::uint32_t clientTimeouts = 0;
};

class WebUiService {
public:
    static constexpr std::uint16_t kPort = 80;

    static constexpr std::size_t
        kRequestBufferBytes = 1536;

    static constexpr std::size_t
        kDynamicResponseBytes = 8192;

    static constexpr std::size_t
        kReceiveChunkBytes = 512;

    static constexpr std::size_t
        kSendChunkBytes = 1024;

    static constexpr std::uint64_t
        kClientIdleTimeoutUs = 2000000ULL;

    WebUiService() = default;
    ~WebUiService();

    WebUiService(
        const WebUiService&) = delete;

    WebUiService& operator=(
        const WebUiService&) = delete;

    bool begin();
    void stop();

    WebUiActionEvent poll(
        WebUiSnapshotProvider snapshotProvider,
        std::uint64_t nowUs);

    bool running() const {
        return listenSocket_ >= 0;
    }

    const WebUiStats& stats() const {
        return stats_;
    }

private:
    bool setNonBlocking(
        int socket);

    void closeClient();
    void resetRequest();

    bool acceptClient(
        std::uint64_t nowUs);

    WebUiActionEvent receiveStep(
        WebUiSnapshotProvider snapshotProvider,
        std::uint64_t nowUs);

    void sendStep(
        std::uint64_t nowUs);

    void selectStaticResponse(
        const char* response,
        std::size_t length);

    void selectDynamicResponse(
        std::size_t length);

    void selectErrorResponse(
        WebUiParseResult result);

    bool buildStatusResponse(
        const WebUiSnapshot& snapshot);

    bool buildQueuedResponse(
        std::uint32_t sequence);

    int listenSocket_ = -1;
    int clientSocket_ = -1;

    std::array<
        char,
        kRequestBufferBytes + 1>
        requestBuffer_{};

    std::size_t requestLength_ = 0;

    std::array<
        char,
        kDynamicResponseBytes>
        dynamicResponse_{};

    const char* responseData_ = nullptr;
    std::size_t responseLength_ = 0;
    std::size_t responseOffset_ = 0;

    std::uint64_t clientLastActivityUs_ = 0;
    std::uint32_t nextActionSequence_ = 0;

    // State-changing requests are released to main only after the browser has
    // received the queued acknowledgement. This is important for Wi-Fi
    // reconfiguration and factory reset, both of which can tear down TCP.
    WebUiActionEvent pendingAction_{};

    WebUiStats stats_{};
};

} // namespace ambilight
