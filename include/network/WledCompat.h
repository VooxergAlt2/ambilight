#pragma once

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
};

} // namespace ambilight
