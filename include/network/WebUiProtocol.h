#pragma once

#include <cstddef>
#include <cstdint>

namespace ambilight {

enum class WebUiRoute : std::uint8_t {
    Unknown = 0,
    Index,
    Status,
    WledCombined,
    WledState,
    WledInfo,
    WledEffects,
    WledPalettes,
    Power,
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
    TofDebug,
    FactoryReset
};

enum class WebUiActionKind : std::uint8_t {
    None = 0,
    WledState,
    Power,
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
    TofDebug,
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

enum class WebUiHttpMethod : std::uint8_t {
    Unknown = 0,
    Get,
    Post,
    Put
};

struct WebUiHttpRequest {
    WebUiRoute route = WebUiRoute::Unknown;
    WebUiActionKind action = WebUiActionKind::None;
    WebUiHttpMethod method =
        WebUiHttpMethod::Unknown;

    std::size_t bodyOffset = 0;
    std::size_t bodyLength = 0;

    bool controlHeaderPresent = false;
    bool jsonContentTypePresent = false;
};

class WebUiProtocol {
public:
    // Existing runtime payload parsers remain capped at 127 bytes. The WLED
    // compatibility facade accepts a larger, still bounded JSON object.
    static constexpr std::size_t
        kMaxRuntimeBodyBytes = 127;

    static constexpr std::size_t
        kMaxWledBodyBytes = 511;

    static constexpr std::size_t
        kMaxBodyBytes = kMaxWledBodyBytes;

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
