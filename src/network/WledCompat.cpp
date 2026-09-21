#include "network/WledCompat.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ambilight {
namespace {

class JsonCursor {
public:
    JsonCursor(
        const char* data,
        std::size_t length)
        : data_(data),
          length_(length) {}

    void skipWhitespace() {
        while (position_ < length_) {
            const char value =
                data_[position_];

            if (value != ' ' &&
                value != '\t' &&
                value != '\r' &&
                value != '\n') {

                break;
            }

            ++position_;
        }
    }

    bool atEnd() {
        skipWhitespace();
        return position_ == length_;
    }

    bool consume(char expected) {
        skipWhitespace();

        if (position_ >= length_ ||
            data_[position_] != expected) {

            return false;
        }

        ++position_;
        return true;
    }

    bool peek(char expected) {
        skipWhitespace();

        return
            position_ < length_ &&
            data_[position_] == expected;
    }

    bool parseString(
        char* output,
        std::size_t capacity) {

        skipWhitespace();

        if (position_ >= length_ ||
            data_[position_] != '"' ||
            output == nullptr ||
            capacity == 0) {

            return false;
        }

        ++position_;

        std::size_t written = 0;
        bool overflow = false;

        while (position_ < length_) {
            const unsigned char value =
                static_cast<unsigned char>(
                    data_[position_++]);

            if (value == '"') {
                if (overflow) {
                    output[0] = '\0';
                } else {
                    output[written] = '\0';
                }

                return true;
            }

            if (value < 0x20U) {
                return false;
            }

            char decoded =
                static_cast<char>(
                    value);

            if (value == '\\') {
                if (position_ >= length_) {
                    return false;
                }

                const char escape =
                    data_[position_++];

                switch (escape) {
                case '"':
                case '\\':
                case '/':
                    decoded = escape;
                    break;
                case 'b':
                    decoded = '\b';
                    break;
                case 'f':
                    decoded = '\f';
                    break;
                case 'n':
                    decoded = '\n';
                    break;
                case 'r':
                    decoded = '\r';
                    break;
                case 't':
                    decoded = '\t';
                    break;
                case 'u':
                    // WLED property names are ASCII. Accept valid unicode
                    // escape syntax for unknown keys without trying to
                    // normalize it into the small comparison buffer.
                    for (std::size_t digit = 0;
                         digit < 4;
                         ++digit) {

                        if (position_ >= length_ ||
                            !isHex(
                                data_[
                                    position_++])) {

                            return false;
                        }
                    }

                    decoded = '?';
                    break;
                default:
                    return false;
                }
            }

            if (written + 1 >= capacity) {
                // Continue validating the JSON string but mark the key as
                // non-comparable. It must never match a supported key after
                // truncation.
                overflow = true;
                continue;
            }

            if (!overflow) {
                output[written++] =
                    decoded;
            }
        }

        return false;
    }

    bool parseBool(bool& value) {
        skipWhitespace();

        if (matchLiteral("true")) {
            value = true;
            return true;
        }

        if (matchLiteral("false")) {
            value = false;
            return true;
        }

        return false;
    }

    bool parseUnsigned(
        std::uint32_t& value) {

        skipWhitespace();

        if (position_ >= length_ ||
            data_[position_] < '0' ||
            data_[position_] > '9') {

            return false;
        }

        std::uint32_t parsed = 0;

        if (data_[position_] == '0') {
            ++position_;

            if (position_ < length_ &&
                data_[position_] >= '0' &&
                data_[position_] <= '9') {

                return false;
            }
        } else {
            while (
                position_ < length_ &&
                data_[position_] >= '0' &&
                data_[position_] <= '9') {

                const std::uint32_t digit =
                    static_cast<std::uint32_t>(
                        data_[position_] -
                        '0');

                if (parsed >
                    (0xFFFFFFFFUL - digit) /
                        10UL) {

                    return false;
                }

                parsed =
                    parsed * 10UL +
                    digit;

                ++position_;
            }
        }

        if (position_ < length_ &&
            (
                data_[position_] == '.' ||
                data_[position_] == 'e' ||
                data_[position_] == 'E'
            )) {

            return false;
        }

        value = parsed;
        return true;
    }

    bool skipValue(
        std::uint8_t depth = 0) {

        if (depth > 8U) {
            return false;
        }

        skipWhitespace();

        if (position_ >= length_) {
            return false;
        }

        switch (data_[position_]) {
        case '"':
            return skipString();
        case '{':
            return skipObject(
                static_cast<std::uint8_t>(
                    depth + 1U));
        case '[':
            return skipArray(
                static_cast<std::uint8_t>(
                    depth + 1U));
        case 't':
            return matchLiteral("true");
        case 'f':
            return matchLiteral("false");
        case 'n':
            return matchLiteral("null");
        default:
            return skipNumber();
        }
    }

private:
    static bool isHex(char value) {
        return
            (value >= '0' && value <= '9') ||
            (value >= 'a' && value <= 'f') ||
            (value >= 'A' && value <= 'F');
    }

    bool matchLiteral(
        const char* literal) {

        const std::size_t count =
            std::strlen(
                literal);

        if (position_ + count >
            length_) {

            return false;
        }

        if (std::memcmp(
                data_ + position_,
                literal,
                count) != 0) {

            return false;
        }

        position_ += count;
        return true;
    }

    bool skipString() {
        if (position_ >= length_ ||
            data_[position_] != '"') {

            return false;
        }

        ++position_;

        while (position_ < length_) {
            const unsigned char value =
                static_cast<unsigned char>(
                    data_[position_++]);

            if (value == '"') {
                return true;
            }

            if (value < 0x20U) {
                return false;
            }

            if (value != '\\') {
                continue;
            }

            if (position_ >= length_) {
                return false;
            }

            const char escape =
                data_[position_++];

            if (escape == 'u') {
                for (std::size_t digit = 0;
                     digit < 4;
                     ++digit) {

                    if (position_ >= length_ ||
                        !isHex(
                            data_[
                                position_++])) {

                        return false;
                    }
                }

                continue;
            }

            if (escape != '"' &&
                escape != '\\' &&
                escape != '/' &&
                escape != 'b' &&
                escape != 'f' &&
                escape != 'n' &&
                escape != 'r' &&
                escape != 't') {

                return false;
            }
        }

        return false;
    }

    bool skipNumber() {
        const std::size_t start =
            position_;

        if (position_ < length_ &&
            data_[position_] == '-') {

            ++position_;
        }

        if (position_ >= length_) {
            return false;
        }

        if (data_[position_] == '0') {
            ++position_;
        } else if (
            data_[position_] >= '1' &&
            data_[position_] <= '9') {

            while (
                position_ < length_ &&
                data_[position_] >= '0' &&
                data_[position_] <= '9') {

                ++position_;
            }
        } else {
            return false;
        }

        if (position_ < length_ &&
            data_[position_] == '.') {

            ++position_;

            if (position_ >= length_ ||
                data_[position_] < '0' ||
                data_[position_] > '9') {

                return false;
            }

            while (
                position_ < length_ &&
                data_[position_] >= '0' &&
                data_[position_] <= '9') {

                ++position_;
            }
        }

        if (position_ < length_ &&
            (
                data_[position_] == 'e' ||
                data_[position_] == 'E'
            )) {

            ++position_;

            if (position_ < length_ &&
                (
                    data_[position_] == '+' ||
                    data_[position_] == '-'
                )) {

                ++position_;
            }

            if (position_ >= length_ ||
                data_[position_] < '0' ||
                data_[position_] > '9') {

                return false;
            }

            while (
                position_ < length_ &&
                data_[position_] >= '0' &&
                data_[position_] <= '9') {

                ++position_;
            }
        }

        return position_ > start;
    }

    bool skipObject(
        std::uint8_t depth) {

        if (!consume('{')) {
            return false;
        }

        if (consume('}')) {
            return true;
        }

        while (true) {
            if (!skipString() ||
                !consume(':') ||
                !skipValue(depth)) {

                return false;
            }

            if (consume('}')) {
                return true;
            }

            if (!consume(',')) {
                return false;
            }
        }
    }

    bool skipArray(
        std::uint8_t depth) {

        if (!consume('[')) {
            return false;
        }

        if (consume(']')) {
            return true;
        }

        while (true) {
            if (!skipValue(depth)) {
                return false;
            }

            if (consume(']')) {
                return true;
            }

            if (!consume(',')) {
                return false;
            }
        }
    }

    const char* data_ = nullptr;
    std::size_t length_ = 0;
    std::size_t position_ = 0;
};

bool keyEquals(
    const char* key,
    const char* expected) {

    return
        key != nullptr &&
        expected != nullptr &&
        std::strcmp(
            key,
            expected) == 0;
}

WledStateParseResult parseSegmentObject(
    JsonCursor& cursor,
    WledStateCommand& command) {

    if (!cursor.consume('{')) {
        return
            WledStateParseResult::
                InvalidJson;
    }

    bool idKnown = false;
    std::uint32_t id = 0;
    bool hasOn = false;
    bool on = false;

    if (cursor.consume('}')) {
        return
            WledStateParseResult::Ok;
    }

    while (true) {
        char key[24]{};

        if (!cursor.parseString(
                key,
                sizeof(key)) ||
            !cursor.consume(':')) {

            return
                WledStateParseResult::
                    InvalidJson;
        }

        if (keyEquals(
                key,
                "id")) {

            if (!cursor.parseUnsigned(
                    id)) {

                return
                    WledStateParseResult::
                        InvalidJson;
            }

            idKnown = true;
        } else if (
            keyEquals(
                key,
                "on")) {

            if (!cursor.parseBool(
                    on)) {

                return
                    WledStateParseResult::
                        InvalidJson;
            }

            hasOn = true;
        } else if (
            keyEquals(
                key,
                "bri")) {

            std::uint32_t brightness = 0;

            if (!cursor.parseUnsigned(
                    brightness)) {

                return
                    WledStateParseResult::
                        InvalidJson;
            }

            if (brightness > 255U) {
                return
                    WledStateParseResult::
                        OutOfRange;
            }

            // Deliberately accepted and ignored. See WledStateCommand.
        } else if (
            !cursor.skipValue()) {

            return
                WledStateParseResult::
                    InvalidJson;
        }

        if (cursor.consume('}')) {
            break;
        }

        if (!cursor.consume(',')) {
            return
                WledStateParseResult::
                    InvalidJson;
        }
    }

    if (
        hasOn &&
        (!idKnown || id == 0U)
    ) {

        command.hasSegmentOn = true;
        command.segmentOn = on;
    }

    return WledStateParseResult::Ok;
}

WledStateParseResult parseSegments(
    JsonCursor& cursor,
    WledStateCommand& command) {

    if (cursor.peek('{')) {
        return parseSegmentObject(
            cursor,
            command);
    }

    if (!cursor.consume('[')) {
        return
            WledStateParseResult::
                InvalidJson;
    }

    if (cursor.consume(']')) {
        return WledStateParseResult::Ok;
    }

    while (true) {
        if (cursor.peek('{')) {
            const auto result =
                parseSegmentObject(
                    cursor,
                    command);

            if (result !=
                WledStateParseResult::Ok) {

                return result;
            }
        } else if (
            !cursor.skipValue()) {

            return
                WledStateParseResult::
                    InvalidJson;
        }

        if (cursor.consume(']')) {
            return WledStateParseResult::Ok;
        }

        if (!cursor.consume(',')) {
            return
                WledStateParseResult::
                    InvalidJson;
        }
    }
}

} // namespace

WledStateParseResult WledCompat::parseStateCommand(
    const char* data,
    std::size_t length,
    WledStateCommand& command) {

    command = {};

    if (data == nullptr ||
        length == 0) {

        return
            WledStateParseResult::Empty;
    }

    JsonCursor cursor(
        data,
        length);

    if (!cursor.consume('{')) {
        return
            WledStateParseResult::
                InvalidJson;
    }

    if (cursor.consume('}')) {
        return
            cursor.atEnd()
                ? WledStateParseResult::Ok
                : WledStateParseResult::
                      InvalidJson;
    }

    while (true) {
        char key[24]{};

        if (!cursor.parseString(
                key,
                sizeof(key)) ||
            !cursor.consume(':')) {

            return
                WledStateParseResult::
                    InvalidJson;
        }

        if (keyEquals(
                key,
                "on")) {

            if (!cursor.parseBool(
                    command.on)) {

                return
                    WledStateParseResult::
                        InvalidJson;
            }

            command.hasOn = true;
        } else if (
            keyEquals(
                key,
                "bri")) {

            std::uint32_t brightness = 0;

            if (!cursor.parseUnsigned(
                    brightness)) {

                return
                    WledStateParseResult::
                        InvalidJson;
            }

            if (brightness > 255U) {
                return
                    WledStateParseResult::
                        OutOfRange;
            }

            command.hasBrightness = true;
            command.brightness =
                static_cast<std::uint8_t>(
                    brightness);
        } else if (
            keyEquals(
                key,
                "v")) {

            if (!cursor.parseBool(
                    command.verbose)) {

                return
                    WledStateParseResult::
                        InvalidJson;
            }
        } else if (
            keyEquals(
                key,
                "live")) {

            if (!cursor.parseBool(
                    command.liveRequested)) {

                return
                    WledStateParseResult::
                        InvalidJson;
            }
        } else if (
            keyEquals(
                key,
                "seg")) {

            const auto result =
                parseSegments(
                    cursor,
                    command);

            if (result !=
                WledStateParseResult::Ok) {

                return result;
            }
        } else if (
            !cursor.skipValue()) {

            return
                WledStateParseResult::
                    InvalidJson;
        }

        if (cursor.consume('}')) {
            break;
        }

        if (!cursor.consume(',')) {
            return
                WledStateParseResult::
                    InvalidJson;
        }
    }

    return
        cursor.atEnd()
            ? WledStateParseResult::Ok
            : WledStateParseResult::
                  InvalidJson;
}

WledResolvedOutputState WledCompat::resolveOutputState(
    bool enabled,
    std::uint8_t brightness,
    std::uint8_t defaultBrightness,
    const WledStateCommand& command) {

    WledResolvedOutputState resolved;
    resolved.enabled = enabled;
    resolved.brightness = brightness;

    if (command.hasBrightness) {
        if (command.brightness == 0) {
            // Match WLED's useful behaviour for bri=0: turn the output off
            // without throwing away the last non-zero brightness.
            resolved.enabled = false;
        } else {
            resolved.brightness =
                command.brightness;
        }
    }

    const bool hasPowerRequest =
        command.hasOn ||
        command.hasSegmentOn;

    const bool powerRequest =
        command.hasOn
            ? command.on
            : command.segmentOn;

    if (hasPowerRequest) {
        resolved.enabled =
            powerRequest;

        if (resolved.enabled &&
            resolved.brightness == 0) {

            resolved.brightness =
                defaultBrightness != 0
                    ? defaultBrightness
                    : 1U;
        }
    }

    // bri=0 is explicitly dark even if a client also sends on=true.
    if (command.hasBrightness &&
        command.brightness == 0) {

        resolved.enabled = false;
    }

    return resolved;
}

std::uint8_t WledCompat::reportedBrightness(
    std::uint8_t configuredBrightness) {

    return
        configuredBrightness == 0
            ? 1U
            : configuredBrightness;
}

std::uint8_t WledCompat::rssiToSignalPercent(
    std::int32_t rssiDbm) {

    if (rssiDbm <= -100) {
        return 0;
    }

    if (rssiDbm >= -50) {
        return 100;
    }

    return
        static_cast<std::uint8_t>(
            2 * (rssiDbm + 100));
}

} // namespace ambilight
