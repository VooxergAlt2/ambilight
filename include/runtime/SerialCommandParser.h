#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ambilight {

enum class SerialCommandKind : std::uint8_t {
    None = 0,

    Correction,
    Commissioning,

    Factory,
    LedMap,
    Spatial,
    GainCurve,
    Wifi,
    Brightness,

    DumpTofRaw,
    DumpTofGeometry,
    DumpTofPlane,
    DumpTofGains,
    DumpSpatialGains,
    StartCalibrationCapture,
    DumpRender,
    StartShadowProbe,
    CorrectionStatus,
    FirmwareStatus
};

enum class SerialCommandError : std::uint8_t {
    None = 0,
    UnsupportedCharacter,
    TooLong
};

struct SerialCommandEvent {
    static constexpr std::size_t kPayloadCapacity = 128;

    SerialCommandKind kind =
        SerialCommandKind::None;

    SerialCommandError error =
        SerialCommandError::None;

    std::array<
        char,
        kPayloadCapacity>
        payload{};

    std::size_t payloadLength = 0;

    bool ready() const {
        return kind !=
            SerialCommandKind::None;
    }

    bool valid() const {
        return
            ready() &&
            error ==
                SerialCommandError::None;
    }

    const char* text() const {
        return payload.data();
    }

    char* text() {
        return payload.data();
    }
};

class SerialCommandParser {
public:
    SerialCommandEvent feed(
        int input);

    void reset();

private:
    enum class State : std::uint8_t {
        Idle = 0,
        AwaitCorrectionArgument,
        AwaitCommissioningArgument,

        LineFactory,
        LineLedMap,
        LineSpatial,
        LineGainCurve,
        LineWifi,
        LineBrightness,

        DiscardUntilEol
    };

    static bool isEol(int input);
    static bool isPrintable(int input);

    static SerialCommandKind kindForState(
        State state);

    static std::size_t capacityForState(
        State state);

    void beginLine(
        State state);

    SerialCommandEvent emitLine();

    SerialCommandEvent emitArgument(
        SerialCommandKind kind,
        int input);

    SerialCommandEvent emitImmediate(
        SerialCommandKind kind);

    SerialCommandEvent emitError(
        SerialCommandKind kind,
        SerialCommandError error);

    State state_ = State::Idle;

    std::array<
        char,
        SerialCommandEvent::kPayloadCapacity>
        buffer_{};

    std::size_t length_ = 0;
};

} // namespace ambilight
