#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

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

bool buildJson(
    ambilight::WledJsonDocument document,
    const ambilight::WledCompatSnapshot& snapshot,
    std::string& json,
    const WledStateCommand* overlay = nullptr) {

    char buffer[2048] = {};
    std::size_t length = 0;

    if (!WledCompat::buildJson(
            document,
            snapshot,
            overlay,
            buffer,
            sizeof(buffer),
            length)) {

        return false;
    }

    if (std::strlen(buffer) !=
        length) {

        return false;
    }

    json.assign(
        buffer,
        length);

    return true;
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

void test_wled_state_json_uses_master_brightness_and_fixed_segment_brightness() {
    ambilight::WledCompatSnapshot snapshot;
    snapshot.outputEnabled = true;
    snapshot.brightness = 91;
    snapshot.defaultBrightness = 32;
    snapshot.ledCount = 780;

    std::string json;

    TEST_ASSERT_TRUE(
        buildJson(
            ambilight::
                WledJsonDocument::
                    State,
            snapshot,
            json));

    TEST_ASSERT_NOT_NULL(
        std::strstr(
            json.c_str(),
            "\"on\":true"));

    TEST_ASSERT_NOT_NULL(
        std::strstr(
            json.c_str(),
            "\"bri\":91"));

    TEST_ASSERT_NOT_NULL(
        std::strstr(
            json.c_str(),
            "\"stop\":780"));

    TEST_ASSERT_NOT_NULL(
        std::strstr(
            json.c_str(),
            "\"seg\":[{\"id\":0"));

    TEST_ASSERT_NOT_NULL(
        std::strstr(
            json.c_str(),
            "\"on\":true,\"bri\":255"));
}

void test_wled_state_json_overlay_matches_resolver_prediction() {
    ambilight::WledCompatSnapshot snapshot;
    snapshot.outputEnabled = true;
    snapshot.brightness = 140;
    snapshot.defaultBrightness = 32;
    snapshot.ledCount = 780;

    WledStateCommand command;
    command.hasOn = true;
    command.on = false;

    std::string json;

    TEST_ASSERT_TRUE(
        buildJson(
            ambilight::
                WledJsonDocument::
                    State,
            snapshot,
            json,
            &command));

    TEST_ASSERT_NOT_NULL(
        std::strstr(
            json.c_str(),
            "\"on\":false"));

    // WLED reports the remembered brightness while off.
    TEST_ASSERT_NOT_NULL(
        std::strstr(
            json.c_str(),
            "\"bri\":140"));
}

void test_wled_info_json_matches_current_ha_and_hyperk_contract() {
    ambilight::WledCompatSnapshot snapshot;
    snapshot.ledCount = 780;
    snapshot.wifiConnected = true;
    snapshot.wifiRssi = -62;
    snapshot.wifiChannel = 6;
    std::strcpy(
        snapshot.wifiMac.data(),
        "a1b2c3d4e5f6");
    std::strcpy(
        snapshot.wifiIp.data(),
        "192.168.1.55");
    snapshot.uptimeSeconds = 1234;
    snapshot.freeHeapBytes = 190000;
    snapshot.ddpLive = true;
    snapshot.senderLocked = true;
    std::strcpy(
        snapshot.senderIp.data(),
        "192.168.1.10");

    std::string json;

    TEST_ASSERT_TRUE(
        buildJson(
            ambilight::
                WledJsonDocument::
                    Info,
            snapshot,
            json));

    const char* required[] = {
        "\"ver\":\"0.15.3\"",
        "\"mac\":\"a1b2c3d4e5f6\"",
        "\"ip\":\"192.168.1.55\"",
        "\"live\":true",
        "\"lm\":\"DDP\"",
        "\"lip\":\"192.168.1.10\"",
        "\"ws\":-1",
        "\"count\":780",
        "\"maxseg\":1",
        "\"lc\":2",
        "\"seglc\":[2]",
        "\"signal\":76",
        "\"channel\":6",
        "\"pmt\":1"
    };

    for (const char* token : required) {
        TEST_ASSERT_NOT_NULL(
            std::strstr(
                json.c_str(),
                token));
    }
}

void test_wled_combined_and_auxiliary_documents_are_self_contained() {
    ambilight::WledCompatSnapshot snapshot;
    snapshot.outputEnabled = true;
    snapshot.brightness = 32;
    snapshot.defaultBrightness = 32;
    snapshot.ledCount = 780;

    std::string combined;

    TEST_ASSERT_TRUE(
        buildJson(
            ambilight::
                WledJsonDocument::
                    Combined,
            snapshot,
            combined));

    TEST_ASSERT_NOT_NULL(
        std::strstr(
            combined.c_str(),
            "\"state\":{"));

    TEST_ASSERT_NOT_NULL(
        std::strstr(
            combined.c_str(),
            "\"info\":{"));

    TEST_ASSERT_NOT_NULL(
        std::strstr(
            combined.c_str(),
            "\"effects\":[\"Solid\"]"));

    TEST_ASSERT_NOT_NULL(
        std::strstr(
            combined.c_str(),
            "\"palettes\":[\"Default\"]"));

    std::string presets;

    TEST_ASSERT_TRUE(
        buildJson(
            ambilight::
                WledJsonDocument::
                    Presets,
            snapshot,
            presets));

    TEST_ASSERT_EQUAL_STRING(
        "{}",
        presets.c_str());
}

void test_wled_json_builder_fails_closed_on_overflow_and_wrong_overlay_document() {
    ambilight::WledCompatSnapshot snapshot;
    snapshot.ledCount = 780;

    char tiny[8] = {};
    std::size_t length = 99;

    TEST_ASSERT_FALSE(
        WledCompat::buildJson(
            ambilight::
                WledJsonDocument::
                    Combined,
            snapshot,
            nullptr,
            tiny,
            sizeof(tiny),
            length));

    TEST_ASSERT_EQUAL_UINT32(
        0,
        length);

    WledStateCommand command;
    command.hasOn = true;
    command.on = false;

    char normal[512] = {};

    TEST_ASSERT_FALSE(
        WledCompat::buildJson(
            ambilight::
                WledJsonDocument::
                    Info,
            snapshot,
            &command,
            normal,
            sizeof(normal),
            length));
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

    RUN_TEST(
        test_wled_state_json_uses_master_brightness_and_fixed_segment_brightness);
    RUN_TEST(
        test_wled_state_json_overlay_matches_resolver_prediction);
    RUN_TEST(
        test_wled_info_json_matches_current_ha_and_hyperk_contract);
    RUN_TEST(
        test_wled_combined_and_auxiliary_documents_are_self_contained);
    RUN_TEST(
        test_wled_json_builder_fails_closed_on_overflow_and_wrong_overlay_document);

    return UNITY_END();
}
