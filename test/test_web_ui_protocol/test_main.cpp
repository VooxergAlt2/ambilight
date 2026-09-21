#include <string>

#include <unity.h>

#include "network/WebUiProtocol.h"

using ambilight::WebUiActionKind;
using ambilight::WebUiHttpMethod;
using ambilight::WebUiHttpRequest;
using ambilight::WebUiParseResult;
using ambilight::WebUiProtocol;
using ambilight::WebUiRoute;

namespace {

WebUiParseResult parse(
    const std::string& request,
    WebUiHttpRequest& parsed) {

    return WebUiProtocol::parse(
        request.data(),
        request.size(),
        parsed);
}

} // namespace

void test_get_index_status_and_wled_routes() {
    struct Case {
        const char* path;
        WebUiRoute route;
    };

    const Case cases[] = {
        {"/", WebUiRoute::Index},
        {"/api/status", WebUiRoute::Status},
        {"/json", WebUiRoute::WledCombined},
        {"/json/state", WebUiRoute::WledState},
        {"/json/info", WebUiRoute::WledInfo},
        {"/json/eff", WebUiRoute::WledEffects},
        {"/json/pal", WebUiRoute::WledPalettes}
    };

    for (const auto& testCase : cases) {
        WebUiHttpRequest request;

        const std::string wire =
            std::string("GET ") +
            testCase.path +
            " HTTP/1.1\r\nHost: ambilight\r\n\r\n";

        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                WebUiParseResult::Ok),
            static_cast<int>(
                parse(
                    wire,
                    request)));

        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                testCase.route),
            static_cast<int>(
                request.route));

        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                WebUiHttpMethod::Get),
            static_cast<int>(
                request.method));
    }
}

void test_runtime_post_still_requires_control_header() {
    WebUiHttpRequest request;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WebUiParseResult::Forbidden),
        static_cast<int>(
            parse(
                "POST /api/brightness HTTP/1.1\r\n"
                "Content-Length: 3\r\n\r\n255",
                request)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WebUiParseResult::Ok),
        static_cast<int>(
            parse(
                "POST /api/brightness HTTP/1.1\r\n"
                "x-ambilight-control: 1\r\n"
                "content-length: 3\r\n\r\n255",
                request)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WebUiActionKind::Brightness),
        static_cast<int>(
            request.action));
}

void test_all_runtime_action_routes_map_to_expected_kind() {
    struct Case {
        const char* path;
        WebUiActionKind kind;
    };

    const Case cases[] = {
        {"/api/power", WebUiActionKind::Power},
        {"/api/brightness", WebUiActionKind::Brightness},
        {"/api/correction", WebUiActionKind::Correction},
        {"/api/test", WebUiActionKind::Commissioning},
        {"/api/led-map", WebUiActionKind::LedMap},
        {"/api/pixel-mask", WebUiActionKind::PixelMask},
        {"/api/spatial", WebUiActionKind::Spatial},
        {"/api/curve", WebUiActionKind::GainCurve},
        {"/api/wifi", WebUiActionKind::Wifi},
        {"/api/calibration", WebUiActionKind::Calibration},
        {"/api/shadow-probe", WebUiActionKind::ShadowProbe},
        {"/api/tof-debug", WebUiActionKind::TofDebug},
        {"/api/factory", WebUiActionKind::FactoryReset}
    };

    for (const auto& testCase : cases) {
        WebUiHttpRequest request;

        const std::string wire =
            std::string("POST ") +
            testCase.path +
            " HTTP/1.1\r\n"
            "X-Ambilight-Control: 1\r\n"
            "Content-Length: 1\r\n\r\nx";

        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                WebUiParseResult::Ok),
            static_cast<int>(
                parse(
                    wire,
                    request)));

        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                testCase.kind),
            static_cast<int>(
                request.action));
    }
}

void test_wled_state_accepts_post_and_put_json_without_custom_header() {
    const char* methods[] = {
        "POST",
        "PUT"
    };

    for (const char* method : methods) {
        WebUiHttpRequest request;
        const std::string body =
            "{\"on\":true,\"bri\":255}";

        const std::string wire =
            std::string(method) +
            " /json/state HTTP/1.1\r\n"
            "Content-Type: application/json; charset=utf-8\r\n"
            "Content-Length: " +
            std::to_string(
                body.size()) +
            "\r\n\r\n" +
            body;

        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                WebUiParseResult::Ok),
            static_cast<int>(
                parse(
                    wire,
                    request)));

        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                WebUiActionKind::WledState),
            static_cast<int>(
                request.action));

        TEST_ASSERT_TRUE(
            request.jsonContentTypePresent);
    }
}

void test_wled_write_requires_json_content_type() {
    WebUiHttpRequest request;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WebUiParseResult::Forbidden),
        static_cast<int>(
            parse(
                "POST /json/state HTTP/1.1\r\n"
                "Content-Type: application/x-www-form-urlencoded\r\n"
                "Content-Length: 2\r\n\r\n{}",
                request)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WebUiParseResult::Forbidden),
        static_cast<int>(
            parse(
                "PUT /json HTTP/1.1\r\n"
                "Content-Length: 2\r\n\r\n{}",
                request)));
}

void test_runtime_payload_limit_remains_127_bytes() {
    WebUiHttpRequest request;

    const std::string maxBody(
        WebUiProtocol::
            kMaxRuntimeBodyBytes,
        '1');

    const std::string accepted =
        "POST /api/curve HTTP/1.1\r\n"
        "X-Ambilight-Control: 1\r\n"
        "Content-Length: 127\r\n\r\n" +
        maxBody;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WebUiParseResult::Ok),
        static_cast<int>(
            parse(
                accepted,
                request)));

    const std::string tooLongBody(
        WebUiProtocol::
            kMaxRuntimeBodyBytes + 1,
        '1');

    const std::string rejected =
        "POST /api/curve HTTP/1.1\r\n"
        "X-Ambilight-Control: 1\r\n"
        "Content-Length: 128\r\n\r\n" +
        tooLongBody;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WebUiParseResult::
                PayloadTooLarge),
        static_cast<int>(
            parse(
                rejected,
                request)));
}

void test_wled_payload_has_separate_511_byte_bound() {
    WebUiHttpRequest request;

    const std::string maxBody =
        std::string("{\"x\":\"") +
        std::string(
            WebUiProtocol::
                kMaxWledBodyBytes - 8,
            'a') +
        "\"}";

    TEST_ASSERT_EQUAL_UINT32(
        WebUiProtocol::kMaxWledBodyBytes,
        maxBody.size());

    const std::string accepted =
        "POST /json/state HTTP/1.1\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 511\r\n\r\n" +
        maxBody;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WebUiParseResult::Ok),
        static_cast<int>(
            parse(
                accepted,
                request)));

    const std::string tooLongBody =
        maxBody + " ";

    const std::string rejected =
        "POST /json/state HTTP/1.1\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 512\r\n\r\n" +
        tooLongBody;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WebUiParseResult::
                PayloadTooLarge),
        static_cast<int>(
            parse(
                rejected,
                request)));
}

void test_incomplete_header_and_body_wait_for_more_data() {
    WebUiHttpRequest request;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WebUiParseResult::Incomplete),
        static_cast<int>(
            parse(
                "GET /api/status HTTP/1.1\r\nHost: x\r\n",
                request)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WebUiParseResult::Incomplete),
        static_cast<int>(
            parse(
                "POST /api/brightness HTTP/1.1\r\n"
                "X-Ambilight-Control: 1\r\n"
                "Content-Length: 3\r\n\r\n25",
                request)));
}

void test_chunked_and_conflicting_headers_are_rejected() {
    WebUiHttpRequest request;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WebUiParseResult::BadRequest),
        static_cast<int>(
            parse(
                "POST /json/state HTTP/1.1\r\n"
                "Content-Type: application/json\r\n"
                "Transfer-Encoding: chunked\r\n\r\n",
                request)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WebUiParseResult::BadRequest),
        static_cast<int>(
            parse(
                "POST /json/state HTTP/1.1\r\n"
                "Content-Type: application/json\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 2\r\n\r\n{}",
                request)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WebUiParseResult::BadRequest),
        static_cast<int>(
            parse(
                "POST /api/brightness HTTP/1.1\r\n"
                "X-Ambilight-Control: 1\r\n"
                "Content-Length: 1\r\n"
                "Content-Length: 2\r\n\r\n1",
                request)));
}

void test_unknown_route_and_wrong_methods_are_distinct() {
    WebUiHttpRequest request;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WebUiParseResult::UnknownRoute),
        static_cast<int>(
            parse(
                "GET /nope HTTP/1.1\r\n\r\n",
                request)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WebUiParseResult::
                MethodNotAllowed),
        static_cast<int>(
            parse(
                "PUT /api/status HTTP/1.1\r\n\r\n",
                request)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WebUiParseResult::
                MethodNotAllowed),
        static_cast<int>(
            parse(
                "POST /json/info HTTP/1.1\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: 2\r\n\r\n{}",
                request)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WebUiParseResult::
                MethodNotAllowed),
        static_cast<int>(
            parse(
                "GET /api/brightness HTTP/1.1\r\n\r\n",
                request)));
}

void test_get_rejects_body_and_writes_require_length() {
    WebUiHttpRequest request;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WebUiParseResult::BadRequest),
        static_cast<int>(
            parse(
                "GET /json HTTP/1.1\r\n"
                "Content-Length: 1\r\n\r\nx",
                request)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WebUiParseResult::BadRequest),
        static_cast<int>(
            parse(
                "POST /api/calibration HTTP/1.1\r\n"
                "X-Ambilight-Control: 1\r\n\r\n",
                request)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WebUiParseResult::BadRequest),
        static_cast<int>(
            parse(
                "POST /json/state HTTP/1.1\r\n"
                "Content-Type: application/json\r\n\r\n",
                request)));
}

void test_control_bytes_and_bad_http_are_rejected() {
    WebUiHttpRequest request;

    std::string wire =
        "POST /json/state HTTP/1.1\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 3\r\n\r\n"
        "{} ";

    wire.back() =
        static_cast<char>(1);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WebUiParseResult::BadRequest),
        static_cast<int>(
            parse(
                wire,
                request)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WebUiParseResult::BadRequest),
        static_cast<int>(
            parse(
                "GET / HTTP/2\r\n\r\n",
                request)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WebUiParseResult::BadRequest),
        static_cast<int>(
            parse(
                "POST /api/brightness HTTP/1.1\r\n"
                "X-Ambilight-Control: 1\r\n"
                "Content-Length: abc\r\n\r\n",
                request)));
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(
        test_get_index_status_and_wled_routes);
    RUN_TEST(
        test_runtime_post_still_requires_control_header);
    RUN_TEST(
        test_all_runtime_action_routes_map_to_expected_kind);
    RUN_TEST(
        test_wled_state_accepts_post_and_put_json_without_custom_header);
    RUN_TEST(
        test_wled_write_requires_json_content_type);
    RUN_TEST(
        test_runtime_payload_limit_remains_127_bytes);
    RUN_TEST(
        test_wled_payload_has_separate_511_byte_bound);
    RUN_TEST(
        test_incomplete_header_and_body_wait_for_more_data);
    RUN_TEST(
        test_chunked_and_conflicting_headers_are_rejected);
    RUN_TEST(
        test_unknown_route_and_wrong_methods_are_distinct);
    RUN_TEST(
        test_get_rejects_body_and_writes_require_length);
    RUN_TEST(
        test_control_bytes_and_bad_http_are_rejected);

    return UNITY_END();
}
