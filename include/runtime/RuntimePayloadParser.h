#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "led/LedMappingProfile.h"
#include "led/LedPixelMaskProfile.h"
#include "render/ManualLighting.h"
#include "tof/TofGainModel.h"
#include "tof/TofSpatialProfile.h"

namespace ambilight {

enum class CommissioningRangeTarget : std::uint8_t {
    LogicalSide = 0,
    RawGpio
};

struct CommissioningRangeRequest {
    CommissioningRangeTarget target =
        CommissioningRangeTarget::LogicalSide;

    std::uint8_t targetValue = 0;
    std::uint16_t start = 0;
    std::uint16_t count = 0;
};

enum class RuntimePayloadParseResult : std::uint8_t {
    Ok = 0,
    Empty,
    InvalidFormat,
    OutOfRange
};

class RuntimePayloadParser {
public:
    static RuntimePayloadParseResult parseBrightness(
        const char* text,
        std::uint8_t& brightness);

    static RuntimePayloadParseResult parseManualLighting(
        const char* text,
        ManualLightingState& state);

    static RuntimePayloadParseResult parseCommissioningRange(
        const char* text,
        CommissioningRangeRequest& request);

    static RuntimePayloadParseResult parseLedMapping(
        const char* text,
        LedMappingProfile& profile);

    static RuntimePayloadParseResult parseLedPixelMask(
        const char* text,
        LedPixelMaskProfile& profile);

    static RuntimePayloadParseResult parseSpatialProfile(
        const char* text,
        TofSpatialProfile& profile);

    static RuntimePayloadParseResult parseGainCurve(
        const char* text,
        std::array<
            GainPoint,
            DistanceGainCurve::kMaxPoints>& points,
        std::size_t& count);

private:
    static bool parseUint16(
        const char*& cursor,
        std::uint16_t& value);

    static bool parseDecimalX10(
        const char*& cursor,
        std::int32_t& valueX10);

    static bool consume(
        const char*& cursor,
        char expected);
};

} // namespace ambilight
