#include <cstring>
#include <string>

#include <unity.h>

#include "runtime/SerialCommandParser.h"

using ambilight::SerialCommandError;
using ambilight::SerialCommandEvent;
using ambilight::SerialCommandKind;
using ambilight::SerialCommandParser;

namespace {

SerialCommandEvent feedText(
    SerialCommandParser& parser,
    const std::string& text) {

    SerialCommandEvent last;

    for (const unsigned char ch :
         text) {

        const auto event =
            parser.feed(
                static_cast<int>(ch));

        if (event.ready()) {
            last = event;
        }
    }

    return last;
}

void assertEvent(
    const SerialCommandEvent& event,
    SerialCommandKind kind,
    const char* payload = "") {

    TEST_ASSERT_TRUE(
        event.ready());

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(kind),
        static_cast<int>(
            event.kind));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            SerialCommandError::None),
        static_cast<int>(
            event.error));

    TEST_ASSERT_EQUAL_STRING(
        payload,
        event.text());

    TEST_ASSERT_EQUAL_UINT32(
        std::strlen(payload),
        event.payloadLength);
}

} // namespace

void test_idle_newlines_and_unknown_bytes_are_ignored() {
    SerialCommandParser parser;

    TEST_ASSERT_FALSE(
        parser.feed('\r').ready());

    TEST_ASSERT_FALSE(
        parser.feed('\n').ready());

    TEST_ASSERT_FALSE(
        parser.feed('?').ready());

    TEST_ASSERT_FALSE(
        parser.feed(1).ready());
}

void test_immediate_debug_commands_are_case_insensitive() {
    SerialCommandParser parser;

    assertEvent(
        parser.feed('t'),
        SerialCommandKind::DumpTofRaw);

    assertEvent(
        parser.feed('G'),
        SerialCommandKind::DumpTofGeometry);

    assertEvent(
        parser.feed('p'),
        SerialCommandKind::DumpTofPlane);

    assertEvent(
        parser.feed('K'),
        SerialCommandKind::DumpTofGains);

    assertEvent(
        parser.feed('s'),
        SerialCommandKind::DumpSpatialGains);

    assertEvent(
        parser.feed('C'),
        SerialCommandKind::StartCalibrationCapture);

    assertEvent(
        parser.feed('r'),
        SerialCommandKind::DumpRender);

    assertEvent(
        parser.feed('X'),
        SerialCommandKind::StartShadowProbe);

    assertEvent(
        parser.feed('z'),
        SerialCommandKind::TofDebugToggle);

    assertEvent(
        parser.feed('m'),
        SerialCommandKind::CorrectionStatus);

    assertEvent(
        parser.feed('V'),
        SerialCommandKind::FirmwareStatus);
}

void test_correction_waits_for_exactly_one_argument_byte() {
    SerialCommandParser parser;

    TEST_ASSERT_FALSE(
        parser.feed('!').ready());

    assertEvent(
        parser.feed('2'),
        SerialCommandKind::Correction,
        "2");

    // The CRLF typed after !2 is idle noise and must not emit anything else.
    TEST_ASSERT_FALSE(
        parser.feed('\r').ready());

    TEST_ASSERT_FALSE(
        parser.feed('\n').ready());
}

void test_empty_correction_argument_is_preserved() {
    SerialCommandParser parser;

    parser.feed('!');

    assertEvent(
        parser.feed('\r'),
        SerialCommandKind::Correction,
        "");

    TEST_ASSERT_FALSE(
        parser.feed('\n').ready());
}

void test_commissioning_argument_and_status_framing() {
    SerialCommandParser parser;

    parser.feed('i');

    assertEvent(
        parser.feed('1'),
        SerialCommandKind::Commissioning,
        "1");

    parser.feed('I');

    assertEvent(
        parser.feed('\n'),
        SerialCommandKind::Commissioning,
        "");
}

void test_wifi_line_payload_is_emitted_on_enter() {
    SerialCommandParser parser;

    const auto event =
        feedText(
            parser,
            "wMySSID|MyPassword\r");

    assertEvent(
        event,
        SerialCommandKind::Wifi,
        "MySSID|MyPassword");

    TEST_ASSERT_FALSE(
        parser.feed('\n').ready());
}

void test_wifi_accepts_full_32_plus_63_character_payload() {
    SerialCommandParser parser;

    const std::string ssid(
        32,
        'S');

    const std::string password(
        63,
        'P');

    const std::string command =
        "w" +
        ssid +
        "|" +
        password;

    SerialCommandEvent event;

    for (const unsigned char ch :
         command) {

        const auto current =
            parser.feed(
                static_cast<int>(ch));

        TEST_ASSERT_FALSE(
            current.ready());
    }

    event =
        parser.feed('\n');

    TEST_ASSERT_TRUE(
        event.valid());

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            SerialCommandKind::Wifi),
        static_cast<int>(
            event.kind));

    TEST_ASSERT_EQUAL_UINT32(
        96,
        event.payloadLength);

    TEST_ASSERT_EQUAL_STRING(
        (ssid + "|" + password).c_str(),
        event.text());
}

void test_empty_line_commands_emit_status_events() {
    SerialCommandParser parser;

    parser.feed('w');
    assertEvent(
        parser.feed('\n'),
        SerialCommandKind::Wifi,
        "");

    parser.feed('q');
    assertEvent(
        parser.feed('\n'),
        SerialCommandKind::GainCurve,
        "");

    parser.feed('y');
    assertEvent(
        parser.feed('\n'),
        SerialCommandKind::Spatial,
        "");

    parser.feed('l');
    assertEvent(
        parser.feed('\n'),
        SerialCommandKind::LedMap,
        "");

    parser.feed('d');
    assertEvent(
        parser.feed('\n'),
        SerialCommandKind::LedPixelMask,
        "");

    parser.feed('b');
    assertEvent(
        parser.feed('\n'),
        SerialCommandKind::Brightness,
        "");

    parser.feed('f');
    assertEvent(
        parser.feed('\n'),
        SerialCommandKind::Factory,
        "");
}

void test_commissioning_range_line_payload_is_emitted() {
    SerialCommandParser parser;

    const auto side =
        feedText(
            parser,
            "jside:2:100:10\r");

    assertEvent(
        side,
        SerialCommandKind::CommissioningRange,
        "side:2:100:10");

    const auto gpio =
        feedText(
            parser,
            "Jgpio:20:0:37\n");

    assertEvent(
        gpio,
        SerialCommandKind::CommissioningRange,
        "gpio:20:0:37");
}

void test_disabled_pixel_line_payload_is_emitted() {
    SerialCommandParser parser;

    const auto event =
        feedText(
            parser,
            "d-,12,-,0\r");

    assertEvent(
        event,
        SerialCommandKind::LedPixelMask,
        "-,12,-,0");
}

void test_line_command_prefix_is_not_reinterpreted_inside_payload() {
    SerialCommandParser parser;

    const auto event =
        feedText(
            parser,
            "q50:4096,wreset,x\n");

    assertEvent(
        event,
        SerialCommandKind::GainCurve,
        "50:4096,wreset,x");
}

void test_brightness_accepts_three_digits() {
    SerialCommandParser parser;

    const auto event =
        feedText(
            parser,
            "b255\r");

    assertEvent(
        event,
        SerialCommandKind::Brightness,
        "255");
}

void test_fourth_brightness_digit_errors_and_discards_to_eol() {
    SerialCommandParser parser;

    parser.feed('b');
    parser.feed('1');
    parser.feed('2');
    parser.feed('3');

    const auto error =
        parser.feed('4');

    TEST_ASSERT_TRUE(
        error.ready());

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            SerialCommandKind::Brightness),
        static_cast<int>(
            error.kind));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            SerialCommandError::TooLong),
        static_cast<int>(
            error.error));

    // The rest of the broken line is ignored, even if it contains a valid
    // immediate command character.
    TEST_ASSERT_FALSE(
        parser.feed('x').ready());

    TEST_ASSERT_FALSE(
        parser.feed('\r').ready());

    assertEvent(
        parser.feed('m'),
        SerialCommandKind::CorrectionStatus);
}

void test_unsupported_control_character_discards_rest_of_line() {
    SerialCommandParser parser;

    parser.feed('q');
    parser.feed('5');
    parser.feed('0');

    const auto error =
        parser.feed(1);

    TEST_ASSERT_TRUE(
        error.ready());

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            SerialCommandKind::GainCurve),
        static_cast<int>(
            error.kind));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            SerialCommandError::
                UnsupportedCharacter),
        static_cast<int>(
            error.error));

    TEST_ASSERT_FALSE(
        parser.feed('x').ready());

    TEST_ASSERT_FALSE(
        parser.feed('\n').ready());

    assertEvent(
        parser.feed('x'),
        SerialCommandKind::StartShadowProbe);
}

void test_gain_curve_max_payload_boundary_is_nul_terminated() {
    SerialCommandParser parser;

    parser.feed('q');

    for (std::size_t index = 0;
         index < 127;
         ++index) {

        TEST_ASSERT_FALSE(
            parser.feed('1').ready());
    }

    const auto event =
        parser.feed('\n');

    TEST_ASSERT_TRUE(
        event.valid());

    TEST_ASSERT_EQUAL_UINT32(
        127,
        event.payloadLength);

    TEST_ASSERT_EQUAL_UINT8(
        0,
        static_cast<std::uint8_t>(
            event.payload[127]));
}

void test_gain_curve_overflow_reports_error_once() {
    SerialCommandParser parser;

    parser.feed('q');

    for (std::size_t index = 0;
         index < 127;
         ++index) {

        parser.feed('1');
    }

    const auto error =
        parser.feed('2');

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            SerialCommandError::TooLong),
        static_cast<int>(
            error.error));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            SerialCommandKind::GainCurve),
        static_cast<int>(
            error.kind));

    TEST_ASSERT_FALSE(
        parser.feed('m').ready());

    TEST_ASSERT_FALSE(
        parser.feed('\n').ready());
}

void test_reset_drops_partial_line() {
    SerialCommandParser parser;

    parser.feed('w');
    parser.feed('S');
    parser.feed('S');
    parser.feed('I');
    parser.feed('D');

    parser.reset();

    assertEvent(
        parser.feed('m'),
        SerialCommandKind::CorrectionStatus);
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_idle_newlines_and_unknown_bytes_are_ignored);
    RUN_TEST(test_immediate_debug_commands_are_case_insensitive);
    RUN_TEST(test_correction_waits_for_exactly_one_argument_byte);
    RUN_TEST(test_empty_correction_argument_is_preserved);
    RUN_TEST(test_commissioning_argument_and_status_framing);
    RUN_TEST(test_wifi_line_payload_is_emitted_on_enter);
    RUN_TEST(test_wifi_accepts_full_32_plus_63_character_payload);
    RUN_TEST(test_empty_line_commands_emit_status_events);
    RUN_TEST(test_commissioning_range_line_payload_is_emitted);
    RUN_TEST(test_disabled_pixel_line_payload_is_emitted);
    RUN_TEST(test_line_command_prefix_is_not_reinterpreted_inside_payload);
    RUN_TEST(test_brightness_accepts_three_digits);
    RUN_TEST(test_fourth_brightness_digit_errors_and_discards_to_eol);
    RUN_TEST(test_unsupported_control_character_discards_rest_of_line);
    RUN_TEST(test_gain_curve_max_payload_boundary_is_nul_terminated);
    RUN_TEST(test_gain_curve_overflow_reports_error_once);
    RUN_TEST(test_reset_drops_partial_line);

    return UNITY_END();
}
