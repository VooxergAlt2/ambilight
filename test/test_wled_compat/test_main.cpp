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

    char buffer[4096] = {};
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

void test_master_on_brightness_and_live_field_parse() {
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

void test_segment_fields_are_validated_without_claiming_master_state() {
    WledStateCommand command;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WledStateParseResult::Ok),
        static_cast<int>(
            parse(
                "{\"seg\":[{\"id\":1,\"on\":false},"
                "{\"id\":0,\"on\":true,\"bri\":255}]}",
                command)));

    TEST_ASSERT_FALSE(
        command.hasOn);

    TEST_ASSERT_FALSE(
        command.hasBrightness);
}

void test_segment_rgb_effect_speed_and_intensity_parse() {
    WledStateCommand command;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WledStateParseResult::Ok),
        static_cast<int>(
            parse(
                "{\"seg\":[{\"id\":0,"
                "\"col\":[[12,34,56]],"
                "\"fx\":3,"
                "\"sx\":201,"
                "\"ix\":77}]}",
                command)));

    TEST_ASSERT_TRUE(
        command.hasColor);

    TEST_ASSERT_EQUAL_UINT8(
        12,
        command.color.r);

    TEST_ASSERT_EQUAL_UINT8(
        34,
        command.color.g);

    TEST_ASSERT_EQUAL_UINT8(
        56,
        command.color.b);

    TEST_ASSERT_TRUE(
        command.hasEffect);

    TEST_ASSERT_EQUAL_UINT8(
        3,
        static_cast<std::uint8_t>(
            command.effect));

    TEST_ASSERT_TRUE(
        command.hasSpeed);

    TEST_ASSERT_EQUAL_UINT8(
        201,
        command.speed);

    TEST_ASSERT_TRUE(
        command.hasIntensity);

    TEST_ASSERT_EQUAL_UINT8(
        77,
        command.intensity);
}

void test_segment_rgbw_primary_is_accepted_and_fifth_channel_rejected() {
    WledStateCommand command;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WledStateParseResult::Ok),
        static_cast<int>(
            parse(
                "{\"seg\":[{\"id\":0,\"col\":[[1,2,3,4],[5,6,7],[8,9,10]]}]}",
                command)));

    TEST_ASSERT_TRUE(
        command.hasColor);

    TEST_ASSERT_EQUAL_UINT8(
        1,
        command.color.r);

    TEST_ASSERT_EQUAL_UINT8(
        2,
        command.color.g);

    TEST_ASSERT_EQUAL_UINT8(
        3,
        command.color.b);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WledStateParseResult::OutOfRange),
        static_cast<int>(
            parse(
                "{\"seg\":[{\"id\":0,\"col\":[[1,2,3,4,5]]}]}",
                command)));
}

void test_nonzero_segment_visuals_are_ignored_after_validation() {
    WledStateCommand command;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WledStateParseResult::Ok),
        static_cast<int>(
            parse(
                "{\"seg\":[{\"id\":1,\"col\":[[9,8,7]],\"fx\":2,\"sx\":200,\"ix\":100}]}",
                command)));

    TEST_ASSERT_FALSE(
        command.hasColor);

    TEST_ASSERT_FALSE(
        command.hasEffect);

    TEST_ASSERT_FALSE(
        command.hasSpeed);

    TEST_ASSERT_FALSE(
        command.hasIntensity);
}

void test_segment_visual_ranges_are_bounded() {
    WledStateCommand command;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WledStateParseResult::OutOfRange),
        static_cast<int>(
            parse(
                "{\"seg\":[{\"id\":0,\"fx\":10}]}",
                command)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WledStateParseResult::OutOfRange),
        static_cast<int>(
            parse(
                "{\"seg\":[{\"id\":0,\"col\":[[256,0,0]]}]}",
                command)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WledStateParseResult::OutOfRange),
        static_cast<int>(
            parse(
                "{\"seg\":[{\"id\":0,\"sx\":256}]}",
                command)));
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

void test_segment_preflight_does_not_change_master_output() {
    WledStateCommand command;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WledStateParseResult::Ok),
        static_cast<int>(
            parse(
                "{\"seg\":[{\"id\":0,\"on\":true,\"bri\":255}],\"v\":true}",
                command)));

    const WledResolvedOutputState state =
        WledCompat::resolveOutputState(
            false,
            80,
            32,
            command);

    TEST_ASSERT_FALSE(
        state.enabled);

    TEST_ASSERT_EQUAL_UINT8(
        80,
        state.brightness);
}

void test_home_assistant_one_segment_sequence_has_no_preflight_flash() {
    bool enabled = false;
    std::uint8_t brightness = 80;

    WledStateCommand segmentPreflight;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WledStateParseResult::Ok),
        static_cast<int>(
            parse(
                "{\"seg\":[{\"id\":0,\"on\":true,\"bri\":255}],\"v\":true}",
                segmentPreflight)));

    auto state =
        WledCompat::resolveOutputState(
            enabled,
            brightness,
            32,
            segmentPreflight);

    // HA sends this request first. It must not illuminate the strip at the
    // remembered brightness while the authoritative master request is still
    // in flight.
    TEST_ASSERT_FALSE(
        state.enabled);

    TEST_ASSERT_EQUAL_UINT8(
        80,
        state.brightness);

    enabled = state.enabled;
    brightness = state.brightness;

    WledStateCommand masterOn;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WledStateParseResult::Ok),
        static_cast<int>(
            parse(
                "{\"on\":true,\"bri\":120,\"v\":true}",
                masterOn)));

    state =
        WledCompat::resolveOutputState(
            enabled,
            brightness,
            32,
            masterOn);

    TEST_ASSERT_TRUE(
        state.enabled);

    TEST_ASSERT_EQUAL_UINT8(
        120,
        state.brightness);

    enabled = state.enabled;
    brightness = state.brightness;

    WledStateCommand masterOff;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WledStateParseResult::Ok),
        static_cast<int>(
            parse(
                "{\"on\":false,\"v\":true}",
                masterOff)));

    state =
        WledCompat::resolveOutputState(
            enabled,
            brightness,
            32,
            masterOff);

    TEST_ASSERT_FALSE(
        state.enabled);

    TEST_ASSERT_EQUAL_UINT8(
        120,
        state.brightness);
}

void test_home_assistant_rgb_then_master_sequence_keeps_manual_owner() {
    ambilight::ManualLightingState manual;

    WledStateCommand segment;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WledStateParseResult::Ok),
        static_cast<int>(
            parse(
                "{\"seg\":[{\"id\":0,\"on\":true,\"col\":[[20,40,60]]}],\"v\":true}",
                segment)));

    manual =
        WledCompat::resolveManualLighting(
            manual,
            segment);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(
            ambilight::
                ManualLightingEffect::
                    Solid),
        static_cast<std::uint8_t>(
            manual.effect));

    TEST_ASSERT_EQUAL_UINT8(
        20,
        manual.color.r);

    TEST_ASSERT_EQUAL_UINT8(
        40,
        manual.color.g);

    TEST_ASSERT_EQUAL_UINT8(
        60,
        manual.color.b);

    WledStateCommand master;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            WledStateParseResult::Ok),
        static_cast<int>(
            parse(
                "{\"on\":true,\"bri\":120,\"v\":true}",
                master)));

    manual =
        WledCompat::resolveManualLighting(
            manual,
            master);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(
            ambilight::
                ManualLightingEffect::
                    Solid),
        static_cast<std::uint8_t>(
            manual.effect));

    TEST_ASSERT_EQUAL_UINT8(
        20,
        manual.color.r);

    TEST_ASSERT_EQUAL_UINT8(
        40,
        manual.color.g);

    TEST_ASSERT_EQUAL_UINT8(
        60,
        manual.color.b);
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
    snapshot.ddpBrightness = 91;
    snapshot.lightingBrightness = 91;
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

void test_wled_state_json_reports_rgb_and_manual_effect() {
    ambilight::WledCompatSnapshot snapshot;
    snapshot.outputEnabled = true;
    snapshot.brightness = 80;
    snapshot.ddpBrightness = 80;
    snapshot.lightingBrightness = 80;
    snapshot.defaultBrightness = 32;
    snapshot.ledCount = 632;
    snapshot.manualLighting.effect =
        ambilight::
            ManualLightingEffect::
                Rainbow;
    snapshot.manualLighting.color =
        ambilight::Rgb8{
            11,
            22,
            33
        };
    snapshot.manualLighting.speed = 144;
    snapshot.manualLighting.intensity = 99;

    std::string json;

    TEST_ASSERT_TRUE(
        buildJson(
            ambilight::
                WledJsonDocument::
                    State,
            snapshot,
            json));

    const char* required[] = {
        "\"stop\":632",
        "\"col\":[[11,22,33]]",
        "\"fx\":2",
        "\"sx\":144",
        "\"ix\":99"
    };

    for (const char* token : required) {
        TEST_ASSERT_NOT_NULL(
            std::strstr(
                json.c_str(),
                token));
    }
}

void test_wled_state_overlay_projects_color_to_solid_unless_effect_is_explicit() {
    ambilight::WledCompatSnapshot snapshot;
    snapshot.outputEnabled = true;
    snapshot.brightness = 80;
    snapshot.ddpBrightness = 80;
    snapshot.lightingBrightness = 80;
    snapshot.defaultBrightness = 32;
    snapshot.ledCount = 632;
    snapshot.manualLighting.effect =
        ambilight::
            ManualLightingEffect::
                Ambilight;

    WledStateCommand color;
    color.hasColor = true;
    color.color =
        ambilight::Rgb8{
            90,
            40,
            10
        };

    std::string json;

    TEST_ASSERT_TRUE(
        buildJson(
            ambilight::
                WledJsonDocument::
                    State,
            snapshot,
            json,
            &color));

    TEST_ASSERT_NOT_NULL(
        std::strstr(
            json.c_str(),
            "\"col\":[[90,40,10]]"));

    TEST_ASSERT_NOT_NULL(
        std::strstr(
            json.c_str(),
            "\"fx\":1"));

    color.hasEffect = true;
    color.effect =
        ambilight::
            ManualLightingEffect::
                Ambilight;

    TEST_ASSERT_TRUE(
        buildJson(
            ambilight::
                WledJsonDocument::
                    State,
            snapshot,
            json,
            &color));

    TEST_ASSERT_NOT_NULL(
        std::strstr(
            json.c_str(),
            "\"fx\":0"));
}

void test_wled_state_json_overlay_matches_resolver_prediction() {
    ambilight::WledCompatSnapshot snapshot;
    snapshot.outputEnabled = true;
    snapshot.brightness = 140;
    snapshot.ddpBrightness = 140;
    snapshot.lightingBrightness = 140;
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

void test_wled_info_json_matches_current_ha_contract() {
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
        "\"vid\":\"2609210\"",
        "\"arch\":\"ESP32-C6\"",
        "\"fxcount\":10",
        "\"palcount\":1",
        "\"mac\":\"a1b2c3d4e5f6\"",
        "\"ip\":\"192.168.1.55\"",
        "\"live\":true",
        "\"lm\":\"DDP\"",
        "\"lip\":\"192.168.1.10\"",
        "\"ws\":-1",
        "\"count\":780",
        "\"maxseg\":1",
        "\"lc\":1",
        "\"seglc\":[1]",
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
    snapshot.ddpBrightness = 32;
    snapshot.lightingBrightness = 32;
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
            "\"effects\":[\"Ambilight\",\"Solid\",\"Rainbow\",\"Breathing\",\"Warm White\",\"Bias White\",\"Sunset\",\"Candle\",\"Aurora\",\"Twinkle\"]"));

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
        "{\"0\":{}}",
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



void test_effect_only_overlay_projects_brightness_from_new_owner_bank() {
    ambilight::WledCompatSnapshot snapshot;
    snapshot.outputEnabled = true;
    snapshot.brightness = 180;
    snapshot.ddpBrightness = 180;
    snapshot.lightingBrightness = 42;
    snapshot.defaultBrightness = 32;
    snapshot.ledCount = 780;
    snapshot.manualLighting.effect =
        ambilight::ManualLightingEffect::Ambilight;

    WledStateCommand command;
    command.hasEffect = true;
    command.effect =
        ambilight::ManualLightingEffect::Aurora;

    std::string json;
    TEST_ASSERT_TRUE(
        buildJson(
            ambilight::WledJsonDocument::State,
            snapshot,
            json,
            &command));

    TEST_ASSERT_NOT_NULL(
        std::strstr(
            json.c_str(),
            "\"bri\":42"));
    TEST_ASSERT_NOT_NULL(
        std::strstr(
            json.c_str(),
            "\"fx\":8"));

    snapshot.manualLighting.effect =
        ambilight::ManualLightingEffect::Aurora;
    command.effect =
        ambilight::ManualLightingEffect::Ambilight;

    TEST_ASSERT_TRUE(
        buildJson(
            ambilight::WledJsonDocument::State,
            snapshot,
            json,
            &command));

    TEST_ASSERT_NOT_NULL(
        std::strstr(
            json.c_str(),
            "\"bri\":180"));
    TEST_ASSERT_NOT_NULL(
        std::strstr(
            json.c_str(),
            "\"fx\":0"));
}

void test_explicit_effect_becomes_auto_fallback() {
    ambilight::ManualLightingState current;
    current.effect = ambilight::ManualLightingEffect::Ambilight;
    current.fallbackEffect = ambilight::ManualLightingEffect::BiasWhite;

    WledStateCommand command;
    command.hasEffect = true;
    command.effect = ambilight::ManualLightingEffect::Aurora;

    const auto local =
        WledCompat::resolveManualLighting(
            current,
            command);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(
            ambilight::ManualLightingEffect::Aurora),
        static_cast<std::uint8_t>(local.effect));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(
            ambilight::ManualLightingEffect::Aurora),
        static_cast<std::uint8_t>(local.fallbackEffect));

    command.effect = ambilight::ManualLightingEffect::Ambilight;
    const auto automatic =
        WledCompat::resolveManualLighting(
            local,
            command);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(
            ambilight::ManualLightingEffect::Ambilight),
        static_cast<std::uint8_t>(automatic.effect));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(
            ambilight::ManualLightingEffect::Aurora),
        static_cast<std::uint8_t>(automatic.fallbackEffect));
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(
        test_master_on_brightness_and_live_field_parse);
    RUN_TEST(
        test_pywled_verbose_and_transition_fields_are_accepted);
    RUN_TEST(
        test_segment_fields_are_validated_without_claiming_master_state);
    RUN_TEST(
        test_segment_rgb_effect_speed_and_intensity_parse);
    RUN_TEST(
        test_segment_rgbw_primary_is_accepted_and_fifth_channel_rejected);
    RUN_TEST(
        test_nonzero_segment_visuals_are_ignored_after_validation);
    RUN_TEST(
        test_segment_visual_ranges_are_bounded);
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
        test_segment_preflight_does_not_change_master_output);
    RUN_TEST(
        test_home_assistant_one_segment_sequence_has_no_preflight_flash);
    RUN_TEST(
        test_home_assistant_rgb_then_master_sequence_keeps_manual_owner);
    RUN_TEST(test_effect_only_overlay_projects_brightness_from_new_owner_bank);
    RUN_TEST(test_explicit_effect_becomes_auto_fallback);
    RUN_TEST(
        test_reported_brightness_and_signal_are_wled_safe);

    RUN_TEST(
        test_wled_state_json_uses_master_brightness_and_fixed_segment_brightness);
    RUN_TEST(
        test_wled_state_json_reports_rgb_and_manual_effect);
    RUN_TEST(
        test_wled_state_overlay_projects_color_to_solid_unless_effect_is_explicit);
    RUN_TEST(
        test_wled_state_json_overlay_matches_resolver_prediction);
    RUN_TEST(
        test_wled_info_json_matches_current_ha_contract);
    RUN_TEST(
        test_wled_combined_and_auxiliary_documents_are_self_contained);
    RUN_TEST(
        test_wled_json_builder_fails_closed_on_overflow_and_wrong_overlay_document);

    return UNITY_END();
}
