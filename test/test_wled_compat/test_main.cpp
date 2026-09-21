#include <cstddef>
#include <cstdint>
#include <cstring>

#include <unity.h>

#include "network/WledCompat.h"

using ambilight::WledCompat;
using ambilight::WledResolvedOutputState;
using ambilight::WledStateCommand;
using ambilight::WledStateParseResult;

namespace {

WledStateParseResult parse(
    const char* json,
    WledStateCommand& command) {

    return
        WledCompat::parseStateCommand(
            json,
            std::strlen(json),
            command);
}

} // namespace

void test_master_on_brightness_and_hyperhdr_live_parse() {
    WledStateCommand command;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WledStateParseResult::Ok),
        static_cast<int>(
            parse(
                "{\"on\":true,\"live\":true,\"bri\":255}",
                command)));

    TEST_ASSERT_TRUE(command.hasOn);
    TEST_ASSERT_TRUE(command.on);
    TEST_ASSERT_TRUE(command.hasBrightness);
    TEST_ASSERT_EQUAL_UINT8(
        255,
        command.brightness);
    TEST_ASSERT_TRUE(
        command.liveRequested);
}

void test_pywled_verbose_and_transition_fields_are_accepted() {
    WledStateCommand command;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WledStateParseResult::Ok),
        static_cast<int>(
            parse(
                "{\"on\":false,\"tt\":4,\"v\":true}",
                command)));

    TEST_ASSERT_TRUE(command.hasOn);
    TEST_ASSERT_FALSE(command.on);
    TEST_ASSERT_TRUE(command.verbose);
}

void test_segment_zero_on_is_supported_and_other_segment_ignored() {
    WledStateCommand command;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WledStateParseResult::Ok),
        static_cast<int>(
            parse(
                "{\"seg\":[{\"id\":1,\"on\":false},"
                "{\"id\":0,\"on\":true,\"bri\":255}]}",
                command)));

    TEST_ASSERT_TRUE(
        command.hasSegmentOn);
    TEST_ASSERT_TRUE(
        command.segmentOn);
}

void test_unknown_nested_fields_are_safely_skipped() {
    WledStateCommand command;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WledStateParseResult::Ok),
        static_cast<int>(
            parse(
                "{\"this_is_a_deliberately_long_unknown_property_name\":"
                "{\"nested\":[1,true,null,{\"x\":\"a\\\\b\"}]},"
                "\"on\":true}",
                command)));

    TEST_ASSERT_TRUE(command.hasOn);
    TEST_ASSERT_TRUE(command.on);
}

void test_brightness_range_and_malformed_json_are_rejected() {
    WledStateCommand command;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WledStateParseResult::OutOfRange),
        static_cast<int>(
            parse(
                "{\"bri\":256}",
                command)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WledStateParseResult::InvalidJson),
        static_cast<int>(
            parse(
                "{\"on\":true",
                command)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WledStateParseResult::InvalidJson),
        static_cast<int>(
            parse(
                "{\"bri\":12.5}",
                command)));
}

void test_resolve_off_preserves_last_brightness() {
    WledStateCommand command;
    command.hasOn = true;
    command.on = false;

    const WledResolvedOutputState state =
        WledCompat::resolveOutputState(
            true,
            91,
            32,
            command);

    TEST_ASSERT_FALSE(state.enabled);
    TEST_ASSERT_EQUAL_UINT8(
        91,
        state.brightness);
}

void test_resolve_on_restores_default_if_stored_brightness_is_zero() {
    WledStateCommand command;
    command.hasOn = true;
    command.on = true;

    const WledResolvedOutputState state =
        WledCompat::resolveOutputState(
            false,
            0,
            32,
            command);

    TEST_ASSERT_TRUE(state.enabled);
    TEST_ASSERT_EQUAL_UINT8(
        32,
        state.brightness);
}

void test_resolve_brightness_zero_turns_off_without_erasing_memory() {
    WledStateCommand command;
    command.hasOn = true;
    command.on = true;
    command.hasBrightness = true;
    command.brightness = 0;

    const WledResolvedOutputState state =
        WledCompat::resolveOutputState(
            true,
            140,
            32,
            command);

    TEST_ASSERT_FALSE(state.enabled);
    TEST_ASSERT_EQUAL_UINT8(
        140,
        state.brightness);
}

void test_resolve_nonzero_brightness_updates_memory_and_explicit_on() {
    WledStateCommand command;
    command.hasOn = true;
    command.on = true;
    command.hasBrightness = true;
    command.brightness = 200;

    const WledResolvedOutputState state =
        WledCompat::resolveOutputState(
            false,
            40,
            32,
            command);

    TEST_ASSERT_TRUE(state.enabled);
    TEST_ASSERT_EQUAL_UINT8(
        200,
        state.brightness);
}

void test_master_on_takes_precedence_over_segment_on() {
    WledStateCommand command;
    command.hasOn = true;
    command.on = false;
    command.hasSegmentOn = true;
    command.segmentOn = true;

    const WledResolvedOutputState state =
        WledCompat::resolveOutputState(
            true,
            80,
            32,
            command);

    TEST_ASSERT_FALSE(state.enabled);
}

void test_reported_brightness_and_signal_are_wled_safe() {
    TEST_ASSERT_EQUAL_UINT8(
        1,
        WledCompat::reportedBrightness(
            0));

    TEST_ASSERT_EQUAL_UINT8(
        123,
        WledCompat::reportedBrightness(
            123));

    TEST_ASSERT_EQUAL_UINT8(
        0,
        WledCompat::rssiToSignalPercent(
            -100));

    TEST_ASSERT_EQUAL_UINT8(
        50,
        WledCompat::rssiToSignalPercent(
            -75));

    TEST_ASSERT_EQUAL_UINT8(
        100,
        WledCompat::rssiToSignalPercent(
            -50));
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(
        test_master_on_brightness_and_hyperhdr_live_parse);
    RUN_TEST(
        test_pywled_verbose_and_transition_fields_are_accepted);
    RUN_TEST(
        test_segment_zero_on_is_supported_and_other_segment_ignored);
    RUN_TEST(
        test_unknown_nested_fields_are_safely_skipped);
    RUN_TEST(
        test_brightness_range_and_malformed_json_are_rejected);
    RUN_TEST(
        test_resolve_off_preserves_last_brightness);
    RUN_TEST(
        test_resolve_on_restores_default_if_stored_brightness_is_zero);
    RUN_TEST(
        test_resolve_brightness_zero_turns_off_without_erasing_memory);
    RUN_TEST(
        test_resolve_nonzero_brightness_updates_memory_and_explicit_on);
    RUN_TEST(
        test_master_on_takes_precedence_over_segment_on);
    RUN_TEST(
        test_reported_brightness_and_signal_are_wled_safe);

    return UNITY_END();
}
