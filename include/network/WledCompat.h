#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ambilight {

enum class WledStateParseResult : std::uint8_t {
    Ok = 0,
    Empty,
    InvalidJson,
    OutOfRange
};

struct WledStateCommand {
    bool hasOn = false;
    bool on = false;

    bool hasBrightness = false;
    std::uint8_t brightness = 0;

    // WLED clients commonly address the single exposed segment before
    // updating master state. Segment brightness is accepted but intentionally
    // not applied: the facade advertises one full-length brightness-only
    // segment whose segment brightness remains 255.
    bool hasSegmentOn = false;
    bool segmentOn = false;

    bool verbose = false;
    bool liveRequested = false;
};

struct WledResolvedOutputState {
    bool enabled = true;
    std::uint8_t brightness = 1;
};

enum class WledJsonDocument : std::uint8_t {
    Combined = 0,
    State,
    Info,
    Effects,
    Palettes,
    Presets
};

struct WledCompatSnapshot {
    bool outputEnabled = true;
    std::uint8_t brightness = 1;
    std::uint8_t defaultBrightness = 1;
    std::uint16_t ledCount = 0;

    bool wifiConnected = false;
    std::int32_t wifiRssi = 0;
    std::uint8_t wifiChannel = 0;

    std::array<char, 13> wifiMac{};
    std::array<char, 16> wifiIp{};

    std::uint32_t uptimeSeconds = 0;
    std::uint32_t freeHeapBytes = 0;

    bool ddpLive = false;
    bool senderLocked = false;
    std::array<char, 16> senderIp{};
};

class WledCompat {
public:
    static constexpr const char kApiVersion[] =
        "0.15.3";

    static constexpr std::uint8_t
        kBrightnessCapability = 2;

    static WledStateParseResult parseStateCommand(
        const char* data,
        std::size_t length,
        WledStateCommand& command);

    static WledResolvedOutputState resolveOutputState(
        bool enabled,
        std::uint8_t brightness,
        std::uint8_t defaultBrightness,
        const WledStateCommand& command);

    static std::uint8_t reportedBrightness(
        std::uint8_t configuredBrightness);

    static std::uint8_t rssiToSignalPercent(
        std::int32_t rssiDbm);

    static bool buildJson(
        WledJsonDocument document,
        const WledCompatSnapshot& snapshot,
        const WledStateCommand* overlay,
        char* output,
        std::size_t capacity,
        std::size_t& length);
};

} // namespace ambilight
