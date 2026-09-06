#pragma once

#include <cstddef>
#include <cstdint>

namespace ambilight {

enum class WebUiRoute : std::uint8_t {
    Unknown = 0,
    Index,
    Status,
    Brightness,
    Correction,
    Commissioning,
    LedMap,
    PixelMask,
    Spatial,
    GainCurve,
    Wifi,
    Calibration,
    ShadowProbe,
    FactoryReset
};

enum class WebUiActionKind : std::uint8_t {
    None = 0,
    Brightness,
    Correction,
    Commissioning,
    LedMap,
    PixelMask,
    Spatial,
    GainCurve,
    Wifi,
    Calibration,
    ShadowProbe,
    FactoryReset
};

enum class WebUiParseResult : std::uint8_t {
    Ok = 0,
    Incomplete,
    BadRequest,
    PayloadTooLarge,
    MethodNotAllowed,
    UnknownRoute,
    Forbidden
};

struct WebUiHttpRequest {
    WebUiRoute route = WebUiRoute::Unknown;
    WebUiActionKind action = WebUiActionKind::None;

    std::size_t bodyOffset = 0;
    std::size_t bodyLength = 0;

    bool post = false;
    bool controlHeaderPresent = false;
};

class WebUiProtocol {
public:
    // Existing runtime payload parsers accept at most 127 characters.
    static constexpr std::size_t kMaxBodyBytes = 127;

    static WebUiParseResult parse(
        const char* data,
        std::size_t length,
        WebUiHttpRequest& request);

    static WebUiRoute routeForPath(
        const char* path,
        std::size_t length);

    static WebUiActionKind actionForRoute(
        WebUiRoute route);
};

} // namespace ambilight
