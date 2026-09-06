#include "network/WebUiProtocol.h"

#include <cstddef>
#include <cstdint>

namespace ambilight {
namespace {

bool spanEquals(
    const char* data,
    std::size_t length,
    const char* literal) {

    if (data == nullptr ||
        literal == nullptr) {
        return false;
    }

    std::size_t literalLength = 0;
    while (literal[literalLength] != '\0') {
        ++literalLength;
    }

    if (length != literalLength) {
        return false;
    }

    for (std::size_t index = 0;
         index < length;
         ++index) {

        if (data[index] != literal[index]) {
            return false;
        }
    }

    return true;
}

char asciiLower(char value) {
    if (value >= 'A' &&
        value <= 'Z') {

        return static_cast<char>(
            value - 'A' + 'a');
    }

    return value;
}

bool spanEqualsIgnoreCase(
    const char* data,
    std::size_t length,
    const char* literal) {

    if (data == nullptr ||
        literal == nullptr) {
        return false;
    }

    std::size_t literalLength = 0;
    while (literal[literalLength] != '\0') {
        ++literalLength;
    }

    if (length != literalLength) {
        return false;
    }

    for (std::size_t index = 0;
         index < length;
         ++index) {

        if (asciiLower(data[index]) !=
            asciiLower(literal[index])) {

            return false;
        }
    }

    return true;
}

std::size_t findSequence(
    const char* data,
    std::size_t length,
    const char* needle,
    std::size_t needleLength,
    std::size_t start = 0) {

    if (data == nullptr ||
        needle == nullptr ||
        needleLength == 0 ||
        length < needleLength ||
        start > length - needleLength) {

        return length;
    }

    for (std::size_t index = start;
         index + needleLength <= length;
         ++index) {

        bool match = true;

        for (std::size_t offset = 0;
             offset < needleLength;
             ++offset) {

            if (data[index + offset] !=
                needle[offset]) {

                match = false;
                break;
            }
        }

        if (match) {
            return index;
        }
    }

    return length;
}

void trimSpan(
    const char*& data,
    std::size_t& length) {

    while (length > 0 &&
           (*data == ' ' ||
            *data == '\t')) {

        ++data;
        --length;
    }

    while (length > 0 &&
           (data[length - 1] == ' ' ||
            data[length - 1] == '\t')) {

        --length;
    }
}

bool parseSize(
    const char* data,
    std::size_t length,
    std::size_t& value) {

    trimSpan(
        data,
        length);

    if (length == 0) {
        return false;
    }

    std::size_t parsed = 0;

    for (std::size_t index = 0;
         index < length;
         ++index) {

        const char digit =
            data[index];

        if (digit < '0' ||
            digit > '9') {

            return false;
        }

        const std::size_t next =
            parsed * 10U +
            static_cast<std::size_t>(
                digit - '0');

        if (next < parsed) {
            return false;
        }

        parsed = next;
    }

    value = parsed;
    return true;
}

} // namespace

WebUiRoute WebUiProtocol::routeForPath(
    const char* path,
    std::size_t length) {

    if (spanEquals(path, length, "/") ||
        spanEquals(
            path,
            length,
            "/index.html")) {

        return WebUiRoute::Index;
    }

    if (spanEquals(
            path,
            length,
            "/api/status")) {

        return WebUiRoute::Status;
    }

    if (spanEquals(
            path,
            length,
            "/api/brightness")) {

        return WebUiRoute::Brightness;
    }

    if (spanEquals(
            path,
            length,
            "/api/correction")) {

        return WebUiRoute::Correction;
    }

    if (spanEquals(
            path,
            length,
            "/api/test")) {

        return WebUiRoute::Commissioning;
    }

    if (spanEquals(
            path,
            length,
            "/api/led-map")) {

        return WebUiRoute::LedMap;
    }

    if (spanEquals(
            path,
            length,
            "/api/spatial")) {

        return WebUiRoute::Spatial;
    }

    if (spanEquals(
            path,
            length,
            "/api/curve")) {

        return WebUiRoute::GainCurve;
    }

    if (spanEquals(
            path,
            length,
            "/api/wifi")) {

        return WebUiRoute::Wifi;
    }

    if (spanEquals(
            path,
            length,
            "/api/calibration")) {

        return WebUiRoute::Calibration;
    }

    if (spanEquals(
            path,
            length,
            "/api/shadow-probe")) {

        return WebUiRoute::ShadowProbe;
    }

    if (spanEquals(
            path,
            length,
            "/api/factory")) {

        return WebUiRoute::FactoryReset;
    }

    return WebUiRoute::Unknown;
}

WebUiActionKind WebUiProtocol::actionForRoute(
    WebUiRoute route) {

    switch (route) {
    case WebUiRoute::Brightness:
        return WebUiActionKind::Brightness;
    case WebUiRoute::Correction:
        return WebUiActionKind::Correction;
    case WebUiRoute::Commissioning:
        return WebUiActionKind::Commissioning;
    case WebUiRoute::LedMap:
        return WebUiActionKind::LedMap;
    case WebUiRoute::Spatial:
        return WebUiActionKind::Spatial;
    case WebUiRoute::GainCurve:
        return WebUiActionKind::GainCurve;
    case WebUiRoute::Wifi:
        return WebUiActionKind::Wifi;
    case WebUiRoute::Calibration:
        return WebUiActionKind::Calibration;
    case WebUiRoute::ShadowProbe:
        return WebUiActionKind::ShadowProbe;
    case WebUiRoute::FactoryReset:
        return WebUiActionKind::FactoryReset;
    default:
        return WebUiActionKind::None;
    }
}

WebUiParseResult WebUiProtocol::parse(
    const char* data,
    std::size_t length,
    WebUiHttpRequest& request) {

    request = {};

    if (data == nullptr ||
        length == 0) {

        return WebUiParseResult::Incomplete;
    }

    static constexpr char kHeaderEnd[] =
        "\r\n\r\n";

    static constexpr char kLineEnd[] =
        "\r\n";

    const std::size_t headerEnd =
        findSequence(
            data,
            length,
            kHeaderEnd,
            4);

    if (headerEnd == length) {
        return WebUiParseResult::Incomplete;
    }

    const std::size_t requestLineEnd =
        findSequence(
            data,
            headerEnd,
            kLineEnd,
            2);

    if (requestLineEnd == headerEnd) {
        return WebUiParseResult::BadRequest;
    }

    std::size_t firstSpace =
        requestLineEnd;

    std::size_t secondSpace =
        requestLineEnd;

    for (std::size_t index = 0;
         index < requestLineEnd;
         ++index) {

        if (data[index] != ' ') {
            continue;
        }

        if (firstSpace ==
            requestLineEnd) {

            firstSpace = index;
        } else {
            secondSpace = index;
            break;
        }
    }

    if (firstSpace == requestLineEnd ||
        secondSpace == requestLineEnd ||
        firstSpace == 0 ||
        secondSpace <= firstSpace + 1 ||
        secondSpace + 1 >=
            requestLineEnd) {

        return WebUiParseResult::BadRequest;
    }

    const bool isGet =
        spanEquals(
            data,
            firstSpace,
            "GET");

    const bool isPost =
        spanEquals(
            data,
            firstSpace,
            "POST");

    if (!isGet &&
        !isPost) {

        return
            WebUiParseResult::
                MethodNotAllowed;
    }

    const char* version =
        data + secondSpace + 1;

    const std::size_t versionLength =
        requestLineEnd -
        secondSpace -
        1;

    if (!spanEquals(
            version,
            versionLength,
            "HTTP/1.1") &&
        !spanEquals(
            version,
            versionLength,
            "HTTP/1.0")) {

        return WebUiParseResult::BadRequest;
    }

    request.route =
        routeForPath(
            data + firstSpace + 1,
            secondSpace -
                firstSpace -
                1);

    if (request.route ==
        WebUiRoute::Unknown) {

        return
            WebUiParseResult::
                UnknownRoute;
    }

    request.post = isPost;

    const WebUiActionKind action =
        actionForRoute(
            request.route);

    if (isGet) {
        if (request.route !=
                WebUiRoute::Index &&
            request.route !=
                WebUiRoute::Status) {

            return
                WebUiParseResult::
                    MethodNotAllowed;
        }
    } else {
        if (action ==
            WebUiActionKind::None) {

            return
                WebUiParseResult::
                    MethodNotAllowed;
        }

        request.action = action;
    }

    bool contentLengthSeen = false;
    std::size_t contentLength = 0;
    bool transferEncodingSeen = false;

    std::size_t lineStart =
        requestLineEnd + 2;

    while (lineStart < headerEnd) {
        const std::size_t lineEnd =
            findSequence(
                data,
                headerEnd,
                kLineEnd,
                2,
                lineStart);

        if (lineEnd == headerEnd ||
            lineEnd <= lineStart) {

            return
                WebUiParseResult::
                    BadRequest;
        }

        std::size_t colon = lineEnd;

        for (std::size_t index = lineStart;
             index < lineEnd;
             ++index) {

            if (data[index] == ':') {
                colon = index;
                break;
            }
        }

        if (colon == lineEnd ||
            colon == lineStart) {

            return
                WebUiParseResult::
                    BadRequest;
        }

        const char* name =
            data + lineStart;

        const std::size_t nameLength =
            colon - lineStart;

        const char* value =
            data + colon + 1;

        std::size_t valueLength =
            lineEnd - colon - 1;

        trimSpan(
            value,
            valueLength);

        if (spanEqualsIgnoreCase(
                name,
                nameLength,
                "Content-Length")) {

            std::size_t parsed = 0;

            if (!parseSize(
                    value,
                    valueLength,
                    parsed)) {

                return
                    WebUiParseResult::
                        BadRequest;
            }

            if (contentLengthSeen &&
                contentLength !=
                    parsed) {

                return
                    WebUiParseResult::
                        BadRequest;
            }

            contentLengthSeen = true;
            contentLength = parsed;
        } else if (
            spanEqualsIgnoreCase(
                name,
                nameLength,
                "X-Ambilight-Control")) {

            request.controlHeaderPresent =
                spanEquals(
                    value,
                    valueLength,
                    "1");
        } else if (
            spanEqualsIgnoreCase(
                name,
                nameLength,
                "Transfer-Encoding")) {

            transferEncodingSeen =
                valueLength != 0;
        }

        lineStart =
            lineEnd + 2;
    }

    if (transferEncodingSeen) {
        return
            WebUiParseResult::
                BadRequest;
    }

    if (contentLength >
        kMaxBodyBytes) {

        return
            WebUiParseResult::
                PayloadTooLarge;
    }

    if (isPost) {
        if (!request.controlHeaderPresent) {
            return
                WebUiParseResult::
                    Forbidden;
        }

        if (!contentLengthSeen) {
            return
                WebUiParseResult::
                    BadRequest;
        }
    } else if (
        contentLengthSeen &&
        contentLength != 0) {

        return
            WebUiParseResult::
                BadRequest;
    }

    request.bodyOffset =
        headerEnd + 4;

    request.bodyLength =
        contentLength;

    if (request.bodyOffset >
            length ||
        contentLength >
            length -
                request.bodyOffset) {

        return WebUiParseResult::Incomplete;
    }

    return WebUiParseResult::Ok;
}

} // namespace ambilight
