#include <cstdint>
#include <vector>

#include <unity.h>

#include "transport/AwaParser.h"

using ambilight::AwaEvent;
using ambilight::AwaParser;
using ambilight::RgbFrame;

namespace {

struct Fletcher {
    std::uint16_t one = 0;
    std::uint16_t two = 0;
    std::uint16_t ext = 0;
    std::uint8_t position = 0;

    void add(std::uint8_t value) {
        one = (one + value) % 255U;
        two = (two + one) % 255U;
        ext = (ext + static_cast<std::uint16_t>(value ^ position)) % 255U;
        ++position;
    }

    std::uint8_t extByte() const {
        return static_cast<std::uint8_t>(
            ext != 0x41U ? ext : 0xAAU);
    }
};

std::vector<std::uint8_t> makeFrame(
    bool version2,
    std::uint16_t ledCount = 780) {

    const std::uint16_t encoded =
        static_cast<std::uint16_t>(ledCount - 1);

    std::vector<std::uint8_t> frame;
    frame.reserve(
        6 +
        static_cast<std::size_t>(ledCount) * 3 +
        (version2 ? 4 : 0) +
        3);

    const std::uint8_t hi =
        static_cast<std::uint8_t>((encoded >> 8) & 0xFF);
    const std::uint8_t lo =
        static_cast<std::uint8_t>(encoded & 0xFF);

    frame.push_back('A');
    frame.push_back('w');
    frame.push_back(version2 ? 'A' : 'a');
    frame.push_back(hi);
    frame.push_back(lo);
    frame.push_back(
        static_cast<std::uint8_t>(hi ^ lo ^ 0x55U));

    Fletcher fletcher;

    for (std::size_t index = 0;
         index < static_cast<std::size_t>(ledCount) * 3;
         ++index) {

        const std::uint8_t value =
            static_cast<std::uint8_t>(
                (index * 37U + 11U) & 0xFFU);

        frame.push_back(value);
        fletcher.add(value);
    }

    if (version2) {
        constexpr std::uint8_t calibration[4] = {
            0xFF, 0xA0, 0xB0, 0x70
        };

        for (const auto value : calibration) {
            frame.push_back(value);
            fletcher.add(value);
        }
    }

    frame.push_back(static_cast<std::uint8_t>(fletcher.one));
    frame.push_back(static_cast<std::uint8_t>(fletcher.two));
    frame.push_back(fletcher.extByte());

    return frame;
}

AwaEvent feed(
    AwaParser& parser,
    const std::vector<std::uint8_t>& bytes,
    RgbFrame& frame) {

    AwaEvent last = AwaEvent::None;

    for (const auto value : bytes) {
        const AwaEvent event = parser.consume(value, frame);
        if (event != AwaEvent::None) {
            last = event;
        }
    }

    return last;
}

void assertPattern(const RgbFrame& frame) {
    const auto* bytes =
        reinterpret_cast<const std::uint8_t*>(
            frame.pixels.data());

    for (std::size_t index = 0;
         index < AwaParser::kPayloadBytes;
         ++index) {

        TEST_ASSERT_EQUAL_UINT8(
            static_cast<std::uint8_t>(
                (index * 37U + 11U) & 0xFFU),
            bytes[index]);
    }
}

} // namespace

void test_valid_v1_frame_completes() {
    AwaParser parser;
    RgbFrame frame;

    const auto data = makeFrame(false);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(AwaEvent::FrameComplete),
        static_cast<int>(feed(parser, data, frame)));

    TEST_ASSERT_EQUAL_UINT32(1, parser.stats().goodFrames);
    TEST_ASSERT_EQUAL_UINT32(1, parser.stats().framesStarted);
    TEST_ASSERT_EQUAL_UINT32(0, parser.stats().checksumErrors);

    assertPattern(frame);
}

void test_valid_v2_frame_with_calibration_completes() {
    AwaParser parser;
    RgbFrame frame;

    const auto data = makeFrame(true);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(AwaEvent::FrameComplete),
        static_cast<int>(feed(parser, data, frame)));

    TEST_ASSERT_EQUAL_UINT32(1, parser.stats().goodFrames);
    TEST_ASSERT_EQUAL_UINT32(1, parser.stats().version2Frames);

    assertPattern(frame);
}

void test_handshake_request_is_recognized() {
    AwaParser parser;
    RgbFrame frame;

    const std::vector<std::uint8_t> request = {
        0x41, 0x77, 0x41, 0x2A, 0xA2, 0x15,
        0x68, 0x79, 0x70, 0x65, 0x72, 0x68, 0x64, 0x72
    };

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(AwaEvent::HandshakeRequest),
        static_cast<int>(feed(parser, request, frame)));

    TEST_ASSERT_EQUAL_UINT32(
        1,
        parser.stats().handshakeRequests);
    TEST_ASSERT_EQUAL_UINT32(0, parser.stats().goodFrames);
}

void test_sleep_request_is_recognized() {
    AwaParser parser;
    RgbFrame frame;

    const std::vector<std::uint8_t> request = {
        0x41, 0x77, 0x41, 0x2A, 0xA2, 0x35,
        0x68, 0x79, 0x70, 0x65, 0x72, 0x68, 0x64, 0x72
    };

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(AwaEvent::SleepRequest),
        static_cast<int>(feed(parser, request, frame)));

    TEST_ASSERT_EQUAL_UINT32(1, parser.stats().sleepRequests);
}

void test_wrong_led_count_is_rejected_before_payload() {
    AwaParser parser;
    RgbFrame frame;

    const auto data = makeFrame(false, 100);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(AwaEvent::None),
        static_cast<int>(feed(parser, data, frame)));

    TEST_ASSERT_EQUAL_UINT32(1, parser.stats().wrongLedCount);
    TEST_ASSERT_EQUAL_UINT32(0, parser.stats().goodFrames);
}

void test_corrupted_fletcher_rejects_frame() {
    AwaParser parser;
    RgbFrame frame;

    auto data = makeFrame(false);

    // Fletcher1 is the third byte from the end.
    data[data.size() - 3] ^= 0x01;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(AwaEvent::None),
        static_cast<int>(feed(parser, data, frame)));

    TEST_ASSERT_EQUAL_UINT32(1, parser.stats().checksumErrors);
    TEST_ASSERT_EQUAL_UINT32(0, parser.stats().goodFrames);
}

void test_parser_resynchronizes_on_overlapping_A() {
    AwaParser parser;
    RgbFrame frame;

    auto data = makeFrame(false);
    data.insert(data.begin(), static_cast<std::uint8_t>('A'));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(AwaEvent::FrameComplete),
        static_cast<int>(feed(parser, data, frame)));

    TEST_ASSERT_EQUAL_UINT32(1, parser.stats().goodFrames);
    assertPattern(frame);
}

void test_version3_header_is_rejected() {
    AwaParser parser;
    RgbFrame frame;

    const std::vector<std::uint8_t> data = {
        static_cast<std::uint8_t>('A'),
        static_cast<std::uint8_t>('W'),
        static_cast<std::uint8_t>('a'),
        0x03,
        0x0B,
        0x5D
    };

    feed(parser, data, frame);

    TEST_ASSERT_EQUAL_UINT32(
        1,
        parser.stats().unsupportedVersion3);
    TEST_ASSERT_EQUAL_UINT32(0, parser.stats().goodFrames);
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_valid_v1_frame_completes);
    RUN_TEST(test_valid_v2_frame_with_calibration_completes);
    RUN_TEST(test_handshake_request_is_recognized);
    RUN_TEST(test_sleep_request_is_recognized);
    RUN_TEST(test_wrong_led_count_is_rejected_before_payload);
    RUN_TEST(test_corrupted_fletcher_rejects_frame);
    RUN_TEST(test_parser_resynchronizes_on_overlapping_A);
    RUN_TEST(test_version3_header_is_rejected);

    return UNITY_END();
}
