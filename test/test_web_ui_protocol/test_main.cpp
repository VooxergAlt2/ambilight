#include <string>

#include <unity.h>

#include "network/WebUiProtocol.h"

using ambilight::WebUiActionKind;
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

void test_get_index_and_status_routes() {
    WebUiHttpRequest request;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebUiParseResult::Ok),
        static_cast<int>(
            parse(
                "GET / HTTP/1.1\r\nHost: 192.168.1.10\r\n\r\n",
                request)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebUiRoute::Index),
        static_cast<int>(request.route));

    TEST_ASSERT_FALSE(request.post);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebUiParseResult::Ok),
        static_cast<int>(
            parse(
                "GET /api/status HTTP/1.0\r\n\r\n",
                request)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebUiRoute::Status),
        static_cast<int>(request.route));
}

void test_post_requires_control_header() {
    WebUiHttpRequest request;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebUiParseResult::Forbidden),
        static_cast<int>(
            parse(
                "POST /api/brightness HTTP/1.1\r\n"
                "Content-Length: 3\r\n\r\n255",
                request)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebUiParseResult::Ok),
        static_cast<int>(
            parse(
                "POST /api/brightness HTTP/1.1\r\n"
                "x-ambilight-control: 1\r\n"
                "content-length: 3\r\n\r\n255",
                request)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebUiActionKind::Brightness),
        static_cast<int>(request.action));

    TEST_ASSERT_EQUAL_UINT32(
        3,
        request.bodyLength);
}

void test_all_action_routes_map_to_expected_kind() {
    struct Case {
        const char* path;
        WebUiActionKind kind;
    };

    const Case cases[] = {
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

        const std::string body = "x";

        const std::string wire =
            std::string("POST ") +
            testCase.path +
            " HTTP/1.1\r\n"
            "X-Ambilight-Control: 1\r\n"
            "Content-Length: 1\r\n\r\n" +
            body;

        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(WebUiParseResult::Ok),
            static_cast<int>(
                parse(
                    wire,
                    request)));

        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(testCase.kind),
            static_cast<int>(
                request.action));
    }
}

void test_incomplete_header_and_body_wait_for_more_data() {
    WebUiHttpRequest request;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebUiParseResult::Incomplete),
        static_cast<int>(
            parse(
                "GET /api/status HTTP/1.1\r\nHost: x\r\n",
                request)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebUiParseResult::Incomplete),
        static_cast<int>(
            parse(
                "POST /api/brightness HTTP/1.1\r\n"
                "X-Ambilight-Control: 1\r\n"
                "Content-Length: 3\r\n\r\n25",
                request)));
}

void test_payload_boundary_is_exactly_127_bytes() {
    WebUiHttpRequest request;

    const std::string maxBody(
        WebUiProtocol::kMaxBodyBytes,
        '1');

    const std::string accepted =
        "POST /api/curve HTTP/1.1\r\n"
        "X-Ambilight-Control: 1\r\n"
        "Content-Length: 127\r\n\r\n" +
        maxBody;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebUiParseResult::Ok),
        static_cast<int>(
            parse(
                accepted,
                request)));

    TEST_ASSERT_EQUAL_UINT32(
        127,
        request.bodyLength);

    const std::string tooLongBody(
        WebUiProtocol::kMaxBodyBytes + 1,
        '1');

    const std::string rejected =
        "POST /api/curve HTTP/1.1\r\n"
        "X-Ambilight-Control: 1\r\n"
        "Content-Length: 128\r\n\r\n" +
        tooLongBody;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebUiParseResult::PayloadTooLarge),
        static_cast<int>(
            parse(
                rejected,
                request)));
}

void test_chunked_and_conflicting_lengths_are_rejected() {
    WebUiHttpRequest request;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebUiParseResult::BadRequest),
        static_cast<int>(
            parse(
                "POST /api/brightness HTTP/1.1\r\n"
                "X-Ambilight-Control: 1\r\n"
                "Transfer-Encoding: chunked\r\n\r\n",
                request)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebUiParseResult::BadRequest),
        static_cast<int>(
            parse(
                "POST /api/brightness HTTP/1.1\r\n"
                "X-Ambilight-Control: 1\r\n"
                "Content-Length: 1\r\n"
                "Content-Length: 2\r\n\r\n1",
                request)));
}

void test_unknown_route_and_method_are_distinct() {
    WebUiHttpRequest request;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebUiParseResult::UnknownRoute),
        static_cast<int>(
            parse(
                "GET /nope HTTP/1.1\r\n\r\n",
                request)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebUiParseResult::MethodNotAllowed),
        static_cast<int>(
            parse(
                "PUT /api/status HTTP/1.1\r\n\r\n",
                request)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebUiParseResult::MethodNotAllowed),
        static_cast<int>(
            parse(
                "GET /api/brightness HTTP/1.1\r\n\r\n",
                request)));
}

void test_get_rejects_nonzero_body_and_post_requires_length() {
    WebUiHttpRequest request;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebUiParseResult::BadRequest),
        static_cast<int>(
            parse(
                "GET /api/status HTTP/1.1\r\n"
                "Content-Length: 1\r\n\r\nx",
                request)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebUiParseResult::BadRequest),
        static_cast<int>(
            parse(
                "POST /api/calibration HTTP/1.1\r\n"
                "X-Ambilight-Control: 1\r\n\r\n",
                request)));
}

void test_control_bytes_in_body_are_rejected() {
    WebUiHttpRequest request;

    std::string wire =
        "POST /api/wifi HTTP/1.1\r\n"
        "X-Ambilight-Control: 1\r\n"
        "Content-Length: 3\r\n\r\n"
        "A|B";

    wire.back() =
        static_cast<char>(1);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebUiParseResult::BadRequest),
        static_cast<int>(
            parse(
                wire,
                request)));
}

void test_bad_http_version_and_content_length_are_rejected() {
    WebUiHttpRequest request;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebUiParseResult::BadRequest),
        static_cast<int>(
            parse(
                "GET / HTTP/2\r\n\r\n",
                request)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebUiParseResult::BadRequest),
        static_cast<int>(
            parse(
                "POST /api/brightness HTTP/1.1\r\n"
                "X-Ambilight-Control: 1\r\n"
                "Content-Length: abc\r\n\r\n",
                request)));
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_get_index_and_status_routes);
    RUN_TEST(test_post_requires_control_header);
    RUN_TEST(test_all_action_routes_map_to_expected_kind);
    RUN_TEST(test_incomplete_header_and_body_wait_for_more_data);
    RUN_TEST(test_payload_boundary_is_exactly_127_bytes);
    RUN_TEST(test_chunked_and_conflicting_lengths_are_rejected);
    RUN_TEST(test_unknown_route_and_method_are_distinct);
    RUN_TEST(test_get_rejects_nonzero_body_and_post_requires_length);
    RUN_TEST(test_control_bytes_in_body_are_rejected);
    RUN_TEST(test_bad_http_version_and_content_length_are_rejected);

    return UNITY_END();
}
