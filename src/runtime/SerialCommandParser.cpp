#include "runtime/SerialCommandParser.h"

namespace ambilight {

namespace {

constexpr std::size_t kFactoryCapacity = 16;
constexpr std::size_t kCommissioningRangeCapacity = 48;
constexpr std::size_t kLedMapCapacity = 96;
constexpr std::size_t kLedPixelMaskCapacity = 32;
constexpr std::size_t kSpatialCapacity = 128;
constexpr std::size_t kGainCurveCapacity = 128;

// RuntimeSettings currently allows SSID 32 + separator 1 + password 63.
// One extra byte is reserved for the terminating NUL.
constexpr std::size_t kWifiCapacity = 97;

// b0..b255: three digits plus terminating NUL.
constexpr std::size_t kBrightnessCapacity = 4;

} // namespace

bool SerialCommandParser::isEol(
    int input) {

    return
        input == '\r' ||
        input == '\n';
}

bool SerialCommandParser::isPrintable(
    int input) {

    return
        input >= 32 &&
        input <= 126;
}

SerialCommandKind
SerialCommandParser::kindForState(
    State state) {

    switch (state) {
    case State::LineFactory:
        return SerialCommandKind::Factory;
    case State::LineCommissioningRange:
        return SerialCommandKind::CommissioningRange;
    case State::LineLedMap:
        return SerialCommandKind::LedMap;
    case State::LineLedPixelMask:
        return SerialCommandKind::LedPixelMask;
    case State::LineSpatial:
        return SerialCommandKind::Spatial;
    case State::LineGainCurve:
        return SerialCommandKind::GainCurve;
    case State::LineWifi:
        return SerialCommandKind::Wifi;
    case State::LineBrightness:
        return SerialCommandKind::Brightness;
    default:
        return SerialCommandKind::None;
    }
}

std::size_t
SerialCommandParser::capacityForState(
    State state) {

    switch (state) {
    case State::LineFactory:
        return kFactoryCapacity;
    case State::LineCommissioningRange:
        return kCommissioningRangeCapacity;
    case State::LineLedMap:
        return kLedMapCapacity;
    case State::LineLedPixelMask:
        return kLedPixelMaskCapacity;
    case State::LineSpatial:
        return kSpatialCapacity;
    case State::LineGainCurve:
        return kGainCurveCapacity;
    case State::LineWifi:
        return kWifiCapacity;
    case State::LineBrightness:
        return kBrightnessCapacity;
    default:
        return 0;
    }
}

void SerialCommandParser::reset() {
    state_ = State::Idle;
    length_ = 0;
    buffer_.fill('\0');
}

void SerialCommandParser::beginLine(
    State state) {

    state_ = state;
    length_ = 0;
    buffer_.fill('\0');
}

SerialCommandEvent
SerialCommandParser::emitLine() {

    SerialCommandEvent event;
    event.kind =
        kindForState(
            state_);

    event.payloadLength =
        length_;

    for (std::size_t index = 0;
         index < length_;
         ++index) {

        event.payload[index] =
            buffer_[index];
    }

    event.payload[
        event.payloadLength] = '\0';

    reset();
    return event;
}

SerialCommandEvent
SerialCommandParser::emitArgument(
    SerialCommandKind kind,
    int input) {

    SerialCommandEvent event;
    event.kind = kind;

    if (!isEol(input)) {
        event.payload[0] =
            static_cast<char>(input);

        event.payload[1] = '\0';
        event.payloadLength = 1;
    }

    state_ = State::Idle;
    return event;
}

SerialCommandEvent
SerialCommandParser::emitImmediate(
    SerialCommandKind kind) {

    SerialCommandEvent event;
    event.kind = kind;
    return event;
}

SerialCommandEvent
SerialCommandParser::emitError(
    SerialCommandKind kind,
    SerialCommandError error) {

    SerialCommandEvent event;
    event.kind = kind;
    event.error = error;

    state_ =
        State::DiscardUntilEol;

    length_ = 0;
    buffer_.fill('\0');

    return event;
}

SerialCommandEvent
SerialCommandParser::feed(
    int input) {

    if (state_ ==
        State::DiscardUntilEol) {

        if (isEol(input)) {
            reset();
        }

        return {};
    }

    if (state_ ==
        State::AwaitCorrectionArgument) {

        return emitArgument(
            SerialCommandKind::Correction,
            input);
    }

    if (state_ ==
        State::AwaitCommissioningArgument) {

        return emitArgument(
            SerialCommandKind::Commissioning,
            input);
    }

    if (state_ !=
        State::Idle) {

        const SerialCommandKind kind =
            kindForState(
                state_);

        if (isEol(input)) {
            return emitLine();
        }

        if (!isPrintable(input)) {
            return emitError(
                kind,
                SerialCommandError::
                    UnsupportedCharacter);
        }

        const std::size_t capacity =
            capacityForState(
                state_);

        if (capacity == 0 ||
            length_ + 1 >=
                capacity) {

            return emitError(
                kind,
                SerialCommandError::
                    TooLong);
        }

        buffer_[length_++] =
            static_cast<char>(input);

        buffer_[length_] = '\0';

        return {};
    }

    if (isEol(input)) {
        return {};
    }

    switch (input) {
    case '!':
        state_ =
            State::
                AwaitCorrectionArgument;
        return {};

    case 'i':
    case 'I':
        state_ =
            State::
                AwaitCommissioningArgument;
        return {};

    case 'f':
    case 'F':
        beginLine(
            State::LineFactory);
        return {};

    case 'j':
    case 'J':
        beginLine(
            State::LineCommissioningRange);
        return {};

    case 'l':
    case 'L':
        beginLine(
            State::LineLedMap);
        return {};

    case 'd':
    case 'D':
        beginLine(
            State::LineLedPixelMask);
        return {};

    case 'y':
    case 'Y':
        beginLine(
            State::LineSpatial);
        return {};

    case 'q':
    case 'Q':
        beginLine(
            State::LineGainCurve);
        return {};

    case 'w':
    case 'W':
        beginLine(
            State::LineWifi);
        return {};

    case 'b':
    case 'B':
        beginLine(
            State::LineBrightness);
        return {};

    case 't':
    case 'T':
        return emitImmediate(
            SerialCommandKind::DumpTofRaw);

    case 'g':
    case 'G':
        return emitImmediate(
            SerialCommandKind::
                DumpTofGeometry);

    case 'p':
    case 'P':
        return emitImmediate(
            SerialCommandKind::
                DumpTofPlane);

    case 'k':
    case 'K':
        return emitImmediate(
            SerialCommandKind::
                DumpTofGains);

    case 's':
    case 'S':
        return emitImmediate(
            SerialCommandKind::
                DumpSpatialGains);

    case 'c':
    case 'C':
        return emitImmediate(
            SerialCommandKind::
                StartCalibrationCapture);

    case 'r':
    case 'R':
        return emitImmediate(
            SerialCommandKind::
                DumpRender);

    case 'x':
    case 'X':
        return emitImmediate(
            SerialCommandKind::
                StartShadowProbe);

    case 'z':
    case 'Z':
        return emitImmediate(
            SerialCommandKind::
                TofDebugToggle);

    case 'm':
    case 'M':
        return emitImmediate(
            SerialCommandKind::
                CorrectionStatus);

    case 'v':
    case 'V':
        return emitImmediate(
            SerialCommandKind::
                FirmwareStatus);

    default:
        return {};
    }
}

} // namespace ambilight
