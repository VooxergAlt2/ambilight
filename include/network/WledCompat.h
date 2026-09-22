#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "render/ManualLighting.h"

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

    // The single segment is a structural Home Assistant shim for power /
    // brightness, but it owns manual visual controls (RGB/effect/speed/
    // intensity). Master on/bri remain the sole output power controls.
    bool hasColor = false;
    Rgb8 color{255, 255, 255};

    bool hasEffect = false;
    ManualLightingEffect effect =
        ManualLightingEffect::Ambilight;

    bool hasSpeed = false;
    std::uint8_t speed = 128;

    bool hasIntensity = false;
    std::uint8_t intensity = 128;

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
    std::uint32_t ledCount = 0;

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

    ManualLightingState manualLighting{};
};

class WledCompat {
public:
    static constexpr const char kApiVersion[] =
        "0.15.3";

    static constexpr std::uint8_t
        kRgbCapability = 1;

    static WledStateParseResult parseStateCommand(
        const char* data,
        std::size_t length,
        WledStateCommand& command);

    static WledResolvedOutputState resolveOutputState(
        bool enabled,
        std::uint8_t brightness,
        std::uint8_t defaultBrightness,
        const WledStateCommand& command);

    static ManualLightingState resolveManualLighting(
        const ManualLightingState& current,
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
