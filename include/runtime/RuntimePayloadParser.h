#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "led/LedMappingProfile.h"
#include "tof/TofGainModel.h"
#include "tof/TofSpatialProfile.h"

namespace ambilight {

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

    static RuntimePayloadParseResult parseLedMapping(
        const char* text,
        LedMappingProfile& profile);

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
