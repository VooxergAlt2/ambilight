#include "network/WledCompat.h"

#include <array>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "config/FirmwareInfo.h"

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
    JsonCursor& cursor) {

    if (!cursor.consume('{')) {
        return
            WledStateParseResult::
                InvalidJson;
    }

    std::uint32_t id = 0;
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

            // Segment id is syntax-validated only. The facade exposes one
            // structural segment, while master state owns physical output.
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

            // Accepted for python-wled segment preflight, intentionally not
            // promoted into master output state.
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

    (void)id;
    (void)on;

    return WledStateParseResult::Ok;
}

WledStateParseResult parseSegments(
    JsonCursor& cursor) {

    if (cursor.peek('{')) {
        return parseSegmentObject(
            cursor);
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
                    cursor);

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

class WledJsonWriter {
public:
    WledJsonWriter(
        char* data,
        std::size_t capacity)
        : data_(data),
          capacity_(capacity) {

        if (data_ != nullptr &&
            capacity_ > 0) {

            data_[0] = '\0';
        } else {
            ok_ = false;
        }
    }

    bool append(
        const char* text) {

        if (text == nullptr) {
            ok_ = false;
            return false;
        }

        const std::size_t count =
            std::strlen(text);

        if (!reserve(count)) {
            return false;
        }

        std::memcpy(
            data_ + length_,
            text,
            count);

        length_ += count;
        data_[length_] = '\0';
        return true;
    }

    bool appendf(
        const char* format,
        ...) {

        if (!ok_ ||
            format == nullptr ||
            length_ >= capacity_) {

            ok_ = false;
            return false;
        }

        va_list args;
        va_start(args, format);

        const int written =
            std::vsnprintf(
                data_ + length_,
                capacity_ - length_,
                format,
                args);

        va_end(args);

        if (written < 0 ||
            static_cast<std::size_t>(
                written) >=
                capacity_ - length_) {

            ok_ = false;
            return false;
        }

        length_ +=
            static_cast<std::size_t>(
                written);

        return true;
    }

    bool appendJsonString(
        const char* text) {

        if (!append("\"")) {
            return false;
        }

        if (text != nullptr) {
            static constexpr char kHex[] =
                "0123456789ABCDEF";

            for (std::size_t index = 0;
                 text[index] != '\0';
                 ++index) {

                const unsigned char value =
                    static_cast<unsigned char>(
                        text[index]);

                if (value == '\"' ||
                    value == '\\') {

                    char escaped[3] = {
                        '\\',
                        static_cast<char>(
                            value),
                        '\0'
                    };

                    if (!append(escaped)) {
                        return false;
                    }

                    continue;
                }

                if (value < 0x20U) {
                    char escaped[7] = {
                        '\\',
                        'u',
                        '0',
                        '0',
                        kHex[
                            (value >> 4) &
                            0x0FU],
                        kHex[
                            value &
                            0x0FU],
                        '\0'
                    };

                    if (!append(escaped)) {
                        return false;
                    }

                    continue;
                }

                if (!reserve(1)) {
                    return false;
                }

                data_[length_++] =
                    static_cast<char>(
                        value);

                data_[length_] = '\0';
            }
        }

        return append("\"");
    }

    bool ok() const {
        return ok_;
    }

    std::size_t length() const {
        return length_;
    }

private:
    bool reserve(
        std::size_t extra) {

        if (!ok_ ||
            capacity_ == 0 ||
            length_ >= capacity_ ||
            extra >
                capacity_ -
                    length_ -
                    1U) {

            ok_ = false;
            return false;
        }

        return true;
    }

    char* data_ = nullptr;
    std::size_t capacity_ = 0;
    std::size_t length_ = 0;
    bool ok_ = true;
};

const char* boolJson(
    bool value) {

    return value
        ? "true"
        : "false";
}

WledResolvedOutputState projectedOutputState(
    const WledCompatSnapshot& snapshot,
    const WledStateCommand* overlay) {

    if (overlay == nullptr) {
        WledResolvedOutputState output;
        output.enabled =
            snapshot.outputEnabled;

        output.brightness =
            snapshot.brightness;

        return output;
    }

    return
        WledCompat::resolveOutputState(
            snapshot.outputEnabled,
            snapshot.brightness,
            snapshot.defaultBrightness,
            *overlay);
}

bool appendStateJson(
    WledJsonWriter& writer,
    const WledCompatSnapshot& snapshot,
    const WledStateCommand* overlay) {

    const auto output =
        projectedOutputState(
            snapshot,
            overlay);

    const bool on =
        output.enabled &&
        output.brightness != 0;

    const std::uint8_t brightness =
        WledCompat::reportedBrightness(
            output.brightness);

    writer.appendf(
        "{\"on\":%s,"
        "\"bri\":%u,"
        "\"mainseg\":0,"
        "\"lor\":0,"
        "\"seg\":[{"
        "\"id\":0,"
        "\"start\":0,"
        "\"stop\":%u,"
        "\"on\":%s,"
        "\"bri\":255,"
        "\"fx\":0,"
        "\"pal\":0,"
        "\"sel\":true,"
        "\"cct\":0"
        "}],"
        "\"nl\":{"
        "\"on\":false,"
        "\"dur\":60,"
        "\"mode\":1,"
        "\"tbri\":0"
        "},"
        "\"udpn\":{"
        "\"send\":false,"
        "\"recv\":false"
        "}}",
        boolJson(on),
        static_cast<unsigned>(
            brightness),
        static_cast<unsigned>(
            snapshot.ledCount),
        boolJson(on));

    return writer.ok();
}

bool appendInfoJson(
    WledJsonWriter& writer,
    const WledCompatSnapshot& snapshot) {

    const std::uint8_t signal =
        snapshot.wifiConnected
            ? WledCompat::
                  rssiToSignalPercent(
                      snapshot.wifiRssi)
            : 0U;

    writer.append(
        "{\"ver\":");

    writer.appendJsonString(
        WledCompat::kApiVersion);

    writer.append(
        ",\"vid\":\"2609210\""
        ",\"cn\":");

    writer.appendJsonString(
        config::kFirmwareVersion);

    writer.append(
        ",\"name\":\"Ambilight C6\""
        ",\"brand\":\"Ambilight\""
        ",\"product\":\"ESP32-C6 DDP Ambilight\""
        ",\"arch\":\"ESP32-C6\""
        ",\"mac\":");

    writer.appendJsonString(
        snapshot.wifiMac.data());

    writer.append(
        ",\"ip\":");

    writer.appendJsonString(
        snapshot.wifiIp.data());

    writer.appendf(
        ",\"uptime\":%lu"
        ",\"freeheap\":%lu"
        ",\"live\":%s"
        ",\"lm\":%s"
        ",\"lip\":",
        static_cast<unsigned long>(
            snapshot.uptimeSeconds),
        static_cast<unsigned long>(
            snapshot.freeHeapBytes),
        boolJson(
            snapshot.ddpLive),
        snapshot.ddpLive
            ? "\"DDP\""
            : "\"\"");

    writer.appendJsonString(
        snapshot.ddpLive &&
        snapshot.senderLocked
            ? snapshot.senderIp.data()
            : "");

    writer.appendf(
        ",\"ws\":-1"
        ",\"leds\":{"
        "\"count\":%u,"
        "\"maxseg\":1,"
        "\"lc\":%u,"
        "\"seglc\":[%u],"
        "\"cct\":false,"
        "\"wv\":false,"
        "\"maxpwr\":0,"
        "\"pwr\":0,"
        "\"rgbw\":false"
        "},"
        "\"wifi\":{"
        "\"rssi\":%ld,"
        "\"signal\":%u,"
        "\"channel\":%u"
        "},"
        "\"fs\":{"
        "\"u\":1,"
        "\"t\":2,"
        "\"pmt\":1"
        "}"
        "}",
        static_cast<unsigned>(
            snapshot.ledCount),
        static_cast<unsigned>(
            WledCompat::
                kBrightnessCapability),
        static_cast<unsigned>(
            WledCompat::
                kBrightnessCapability),
        static_cast<long>(
            snapshot.wifiConnected
                ? snapshot.wifiRssi
                : 0),
        static_cast<unsigned>(
            signal),
        static_cast<unsigned>(
            snapshot.wifiChannel));

    return writer.ok();
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
                    cursor);

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

    if (command.hasOn) {
        resolved.enabled =
            command.on;

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

bool WledCompat::buildJson(
    WledJsonDocument document,
    const WledCompatSnapshot& snapshot,
    const WledStateCommand* overlay,
    char* output,
    std::size_t capacity,
    std::size_t& length) {

    length = 0;

    if (output == nullptr ||
        capacity == 0 ||
        (
            overlay != nullptr &&
            document !=
                WledJsonDocument::State
        )) {

        return false;
    }

    WledJsonWriter writer(
        output,
        capacity);

    switch (document) {
    case WledJsonDocument::Combined:
        writer.append(
            "{\"state\":");

        appendStateJson(
            writer,
            snapshot,
            nullptr);

        writer.append(
            ",\"info\":");

        appendInfoJson(
            writer,
            snapshot);

        writer.append(
            ",\"effects\":[\"Solid\"]"
            ",\"palettes\":[\"Default\"]"
            "}");
        break;

    case WledJsonDocument::State:
        appendStateJson(
            writer,
            snapshot,
            overlay);
        break;

    case WledJsonDocument::Info:
        appendInfoJson(
            writer,
            snapshot);
        break;

    case WledJsonDocument::Effects:
        writer.append(
            "[\"Solid\"]");
        break;

    case WledJsonDocument::Palettes:
        writer.append(
            "[\"Default\"]");
        break;

    case WledJsonDocument::Presets:
        // python-wled treats an empty JSON object as a failed presets fetch.
        // Preset id 0 is explicitly discarded by its model layer, so this
        // truthy sentinel means "no user presets" without creating an entity.
        writer.append("{\"0\":{}}");
        break;
    }

    if (!writer.ok()) {
        return false;
    }

    length = writer.length();
    return true;
}

} // namespace ambilight
