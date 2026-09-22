#include "runtime/RuntimePayloadParser.h"

#include <cstring>

namespace ambilight {

bool RuntimePayloadParser::parseUint16(
    const char*& cursor,
    std::uint16_t& value) {

    if (cursor == nullptr ||
        *cursor < '0' ||
        *cursor > '9') {

        return false;
    }

    std::uint32_t parsed = 0;

    while (*cursor >= '0' &&
           *cursor <= '9') {

        parsed =
            parsed * 10U +
            static_cast<std::uint32_t>(
                *cursor - '0');

        if (parsed > 65535U) {
            return false;
        }

        ++cursor;
    }

    value =
        static_cast<std::uint16_t>(
            parsed);

    return true;
}

bool RuntimePayloadParser::parseDecimalX10(
    const char*& cursor,
    std::int32_t& valueX10) {

    if (cursor == nullptr) {
        return false;
    }

    bool negative = false;

    if (*cursor == '-') {
        negative = true;
        ++cursor;
    }

    if (*cursor < '0' ||
        *cursor > '9') {

        return false;
    }

    std::int32_t whole = 0;

    while (*cursor >= '0' &&
           *cursor <= '9') {

        whole =
            whole * 10 +
            static_cast<std::int32_t>(
                *cursor - '0');

        if (whole > 100000) {
            return false;
        }

        ++cursor;
    }

    std::int32_t fraction = 0;

    if (*cursor == '.') {
        ++cursor;

        if (*cursor < '0' ||
            *cursor > '9') {

            return false;
        }

        fraction =
            static_cast<std::int32_t>(
                *cursor - '0');

        ++cursor;

        // Persisted spatial precision is exactly 0.1 mm.
        if (*cursor >= '0' &&
            *cursor <= '9') {

            return false;
        }
    }

    std::int32_t result =
        whole * 10 +
        fraction;

    if (negative) {
        result = -result;
    }

    valueX10 = result;
    return true;
}

bool RuntimePayloadParser::consume(
    const char*& cursor,
    char expected) {

    if (cursor == nullptr ||
        *cursor != expected) {

        return false;
    }

    ++cursor;
    return true;
}

RuntimePayloadParseResult
RuntimePayloadParser::parseCommissioningRange(
    const char* text,
    CommissioningRangeRequest& request) {

    if (text == nullptr ||
        *text == '\0') {

        return
            RuntimePayloadParseResult::Empty;
    }

    CommissioningRangeRequest parsed;
    const char* cursor = text;

    if (std::strncmp(
            cursor,
            "side:",
            5) == 0) {

        parsed.target =
            CommissioningRangeTarget::
                LogicalSide;

        cursor += 5;
    } else if (
        std::strncmp(
            cursor,
            "gpio:",
            5) == 0) {

        parsed.target =
            CommissioningRangeTarget::
                RawGpio;

        cursor += 5;
    } else {
        return
            RuntimePayloadParseResult::
                InvalidFormat;
    }

    std::uint16_t target = 0;
    std::uint16_t start = 0;
    std::uint16_t count = 0;

    if (!parseUint16(
            cursor,
            target) ||
        !consume(
            cursor,
            ':') ||
        !parseUint16(
            cursor,
            start) ||
        !consume(
            cursor,
            ':') ||
        !parseUint16(
            cursor,
            count) ||
        *cursor != '\0') {

        return
            RuntimePayloadParseResult::
                InvalidFormat;
    }

    if (count == 0) {

        return
            RuntimePayloadParseResult::
                OutOfRange;
    }

    if (parsed.target ==
        CommissioningRangeTarget::
            LogicalSide) {

        if (target >=
            static_cast<std::uint16_t>(
                SegmentId::Count)) {

            return
                RuntimePayloadParseResult::
                    OutOfRange;
        }

        parsed.targetValue =
            static_cast<std::uint8_t>(
                target);
    } else {
        std::uint8_t lane = 0;

        if (!LedMappingProfile::laneForGpio(
                target,
                lane)) {

            return
                RuntimePayloadParseResult::
                    OutOfRange;
        }

        parsed.targetValue =
            static_cast<std::uint8_t>(
                target);
    }

    parsed.start = start;
    parsed.count = count;

    request = parsed;

    return
        RuntimePayloadParseResult::Ok;
}

RuntimePayloadParseResult
RuntimePayloadParser::parseBrightness(
    const char* text,
    std::uint8_t& brightness) {

    if (text == nullptr ||
        *text == '\0') {

        return
            RuntimePayloadParseResult::Empty;
    }

    const char* cursor = text;
    std::uint16_t value = 0;

    if (!parseUint16(
            cursor,
            value) ||
        *cursor != '\0') {

        return
            RuntimePayloadParseResult::
                InvalidFormat;
    }

    if (value > 255U) {
        return
            RuntimePayloadParseResult::
                OutOfRange;
    }

    brightness =
        static_cast<std::uint8_t>(
            value);

    return
        RuntimePayloadParseResult::Ok;
}

RuntimePayloadParseResult
RuntimePayloadParser::parseLedMapping(
    const char* text,
    LedMappingProfile& profile) {

    if (text == nullptr ||
        *text == '\0') {

        return
            RuntimePayloadParseResult::Empty;
    }

    LedMappingProfile parsed;
    const char* cursor = text;

    for (std::size_t index = 0;
         index < parsed.segment.size();
         ++index) {

        std::uint16_t length = 0;
        std::uint16_t gpio = 0;
        std::uint16_t reversed = 0;

        if (!parseUint16(
                cursor,
                length) ||
            !consume(
                cursor,
                ':') ||
            !parseUint16(
                cursor,
                gpio) ||
            !consume(
                cursor,
                ':') ||
            !parseUint16(
                cursor,
                reversed)) {

            return
                RuntimePayloadParseResult::
                    InvalidFormat;
        }

        if (length == 0 ||
            reversed > 1U) {

            return
                RuntimePayloadParseResult::
                    OutOfRange;
        }

        std::uint8_t lane = 0;

        if (!LedMappingProfile::laneForGpio(
                gpio,
                lane)) {

            return
                RuntimePayloadParseResult::
                    OutOfRange;
        }

        parsed.segment[index].
            logicalLength =
                length;

        parsed.segment[index].lane =
            lane;

        parsed.segment[index].reversed =
            static_cast<std::uint8_t>(
                reversed);

        if (index + 1 <
            parsed.segment.size()) {

            if (!consume(
                    cursor,
                    ',')) {

                return
                    RuntimePayloadParseResult::
                        InvalidFormat;
            }
        }
    }

    if (*cursor != '\0') {
        return
            RuntimePayloadParseResult::
                InvalidFormat;
    }

    if (!parsed.valid()) {
        return
            RuntimePayloadParseResult::
                OutOfRange;
    }

    profile = parsed;

    return
        RuntimePayloadParseResult::Ok;
}

RuntimePayloadParseResult
RuntimePayloadParser::parseLedPixelMask(
    const char* text,
    LedPixelMaskProfile& profile) {

    if (text == nullptr ||
        *text == '\0') {

        return
            RuntimePayloadParseResult::Empty;
    }

    LedPixelMaskProfile parsed;
    const char* cursor = text;

    for (std::size_t index = 0;
         index < parsed.disabledOffset.size();
         ++index) {

        if (*cursor == '-') {
            parsed.disabledOffset[index] =
                LedPixelMaskProfile::kNone;

            ++cursor;
        } else {
            std::uint16_t offset = 0;

            if (!parseUint16(
                    cursor,
                    offset)) {

                return
                    RuntimePayloadParseResult::
                        InvalidFormat;
            }

            parsed.disabledOffset[index] =
                offset;
        }

        if (index + 1 <
            parsed.disabledOffset.size()) {

            if (!consume(
                    cursor,
                    ',')) {

                return
                    RuntimePayloadParseResult::
                        InvalidFormat;
            }
        }
    }

    if (*cursor != '\0') {
        return
            RuntimePayloadParseResult::
                InvalidFormat;
    }

    if (!parsed.valid()) {
        return
            RuntimePayloadParseResult::
                OutOfRange;
    }

    profile = parsed;

    return
        RuntimePayloadParseResult::Ok;
}

RuntimePayloadParseResult
RuntimePayloadParser::parseSpatialProfile(
    const char* text,
    TofSpatialProfile& profile) {

    if (text == nullptr ||
        *text == '\0') {

        return
            RuntimePayloadParseResult::Empty;
    }

    const char* cursor = text;

    std::int32_t width = 0;
    std::int32_t height = 0;
    std::int32_t sensorX = 0;
    std::int32_t sensorY = 0;
    std::int32_t ledZ = 0;
    std::int32_t deadband = 0;

    std::uint16_t rotation = 0;
    std::uint16_t mirror = 0;

    if (!parseDecimalX10(
            cursor,
            width) ||
        !consume(
            cursor,
            ',') ||
        !parseDecimalX10(
            cursor,
            height) ||
        !consume(
            cursor,
            ',') ||
        !parseDecimalX10(
            cursor,
            sensorX) ||
        !consume(
            cursor,
            ',') ||
        !parseDecimalX10(
            cursor,
            sensorY) ||
        !consume(
            cursor,
            ',') ||
        !parseDecimalX10(
            cursor,
            ledZ) ||
        !consume(
            cursor,
            ',') ||
        !parseUint16(
            cursor,
            rotation) ||
        !consume(
            cursor,
            ',') ||
        !parseUint16(
            cursor,
            mirror) ||
        !consume(
            cursor,
            ',') ||
        !parseDecimalX10(
            cursor,
            deadband) ||
        *cursor != '\0') {

        return
            RuntimePayloadParseResult::
                InvalidFormat;
    }

    if (width < 0 ||
        width > 65535 ||
        height < 0 ||
        height > 65535 ||
        sensorX < -32768 ||
        sensorX > 32767 ||
        sensorY < -32768 ||
        sensorY > 32767 ||
        ledZ < -32768 ||
        ledZ > 32767 ||
        deadband < 0 ||
        deadband > 65535 ||
        rotation > 255 ||
        mirror > 255) {

        return
            RuntimePayloadParseResult::
                OutOfRange;
    }

    TofSpatialProfile parsed;

    parsed.widthMmX10 =
        static_cast<std::uint16_t>(
            width);

    parsed.heightMmX10 =
        static_cast<std::uint16_t>(
            height);

    parsed.sensorOffsetXmmX10 =
        static_cast<std::int16_t>(
            sensorX);

    parsed.sensorOffsetYmmX10 =
        static_cast<std::int16_t>(
            sensorY);

    parsed.ledPlaneZmmX10 =
        static_cast<std::int16_t>(
            ledZ);

    parsed.rotationQuarterTurns =
        static_cast<std::uint8_t>(
            rotation);

    parsed.mirrorX =
        static_cast<std::uint8_t>(
            mirror);

    parsed.planeDeadbandMmX10 =
        static_cast<std::uint16_t>(
            deadband);

    if (!parsed.valid()) {
        return
            RuntimePayloadParseResult::
                OutOfRange;
    }

    profile = parsed;

    return
        RuntimePayloadParseResult::Ok;
}

RuntimePayloadParseResult
RuntimePayloadParser::parseGainCurve(
    const char* text,
    std::array<
        GainPoint,
        DistanceGainCurve::kMaxPoints>& points,
    std::size_t& count) {

    points = {};
    count = 0;

    if (text == nullptr ||
        *text == '\0') {

        return
            RuntimePayloadParseResult::Empty;
    }

    const char* cursor = text;

    while (*cursor != '\0') {
        if (count >=
            points.size()) {

            points = {};
            count = 0;

            return
                RuntimePayloadParseResult::
                    OutOfRange;
        }

        std::uint16_t distance = 0;
        std::uint16_t gain = 0;

        if (!parseUint16(
                cursor,
                distance) ||
            !consume(
                cursor,
                ':') ||
            !parseUint16(
                cursor,
                gain)) {

            points = {};
            count = 0;

            return
                RuntimePayloadParseResult::
                    InvalidFormat;
        }

        points[count++] = {
            distance,
            gain
        };

        if (*cursor == '\0') {
            break;
        }

        if (!consume(
                cursor,
                ',') ||
            *cursor == '\0') {

            points = {};
            count = 0;

            return
                RuntimePayloadParseResult::
                    InvalidFormat;
        }
    }

    const DistanceGainCurve curve(
        points,
        count);

    if (!curve.valid()) {
        points = {};
        count = 0;

        return
            RuntimePayloadParseResult::
                OutOfRange;
    }

    return
        RuntimePayloadParseResult::Ok;
}

} // namespace ambilight
