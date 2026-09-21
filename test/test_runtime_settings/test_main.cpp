#include <unity.h>

#include <Preferences.h>

#include "config/RuntimeSettings.h"

using ambilight::GainPoint;
using ambilight::LedMappingProfile;
using ambilight::LedPixelMaskProfile;
using ambilight::RuntimeSettings;
using ambilight::TofSpatialProfile;

void setUp() {
    Preferences::testReset();
}

void tearDown() {}

void test_schema2_topology_is_invalidated_and_measured_default_wins() {
    Preferences legacy;

    TEST_ASSERT_TRUE(
        legacy.begin(
            "ambilight"));

    LedMappingProfile stale;

    stale.segment[0] = {230, 0, 0};
    stale.segment[1] = {160, 1, 0};
    stale.segment[2] = {230, 2, 0};
    stale.segment[3] = {160, 3, 0};

    TEST_ASSERT_EQUAL_UINT16(
        sizeof(stale),
        legacy.putBytes(
            "led_map",
            &stale,
            sizeof(stale)));

    TEST_ASSERT_EQUAL_UINT16(
        sizeof(std::uint16_t),
        legacy.putUShort(
            "led_map_ver",
            2));

    legacy.end();

    RuntimeSettings settings;

    TEST_ASSERT_TRUE(
        settings.begin());

    TEST_ASSERT_EQUAL_UINT16(
        3,
        LedMappingProfile::
            kSchemaVersion);

    TEST_ASSERT_FALSE(
        settings.
            ledMappingProfileCustomized());

    TEST_ASSERT_FALSE(
        settings.
            ledMappingProfilePersisted());

    TEST_ASSERT_FALSE(
        Preferences::testHasKey(
            "led_map"));

    TEST_ASSERT_FALSE(
        Preferences::testHasKey(
            "led_map_ver"));

    const auto& profile =
        settings.
            ledMappingProfile();

    TEST_ASSERT_EQUAL_UINT8(
        20,
        profile.gpioForSegment(
            ambilight::SegmentId::Top));

    TEST_ASSERT_EQUAL_UINT8(
        19,
        profile.gpioForSegment(
            ambilight::SegmentId::Right));

    TEST_ASSERT_EQUAL_UINT8(
        21,
        profile.gpioForSegment(
            ambilight::SegmentId::Bottom));

    TEST_ASSERT_EQUAL_UINT8(
        18,
        profile.gpioForSegment(
            ambilight::SegmentId::Left));
}

void test_schema3_custom_topology_survives_reboot() {
    LedMappingProfile custom;

    custom.segment[0].logicalLength =
        220;

    custom.segment[1].logicalLength =
        150;

    {
        RuntimeSettings settings;

        TEST_ASSERT_TRUE(
            settings.begin());

        TEST_ASSERT_TRUE(
            settings.setLedMappingProfile(
                custom));
    }

    RuntimeSettings restored;

    TEST_ASSERT_TRUE(
        restored.begin());

    TEST_ASSERT_TRUE(
        restored.
            ledMappingProfileCustomized());

    TEST_ASSERT_TRUE(
        restored.
            ledMappingProfilePersisted());

    TEST_ASSERT_EQUAL_UINT16(
        220,
        restored.
            ledMappingProfile().
            segment[0].
            logicalLength);

    TEST_ASSERT_EQUAL_UINT16(
        150,
        restored.
            ledMappingProfile().
            segment[1].
            logicalLength);

    TEST_ASSERT_EQUAL_UINT8(
        custom.segment[0].lane,
        restored.
            ledMappingProfile().
            segment[0].
            lane);

    TEST_ASSERT_EQUAL_UINT8(
        custom.segment[0].reversed,
        restored.
            ledMappingProfile().
            segment[0].
            reversed);
}

void test_topology_blob_write_failure_does_not_apply_candidate_live() {
    RuntimeSettings settings;
    TEST_ASSERT_TRUE(settings.begin());

    LedMappingProfile previous;
    previous.segment[0].logicalLength = 220;

    TEST_ASSERT_TRUE(
        settings.setLedMappingProfile(
            previous));

    LedMappingProfile candidate =
        previous;

    candidate.segment[0].logicalLength =
        200;

    Preferences::testFailPut(
        "led_map");

    TEST_ASSERT_FALSE(
        settings.setLedMappingProfile(
            candidate));

    TEST_ASSERT_EQUAL_UINT16(
        220,
        settings.
            ledMappingProfile().
            segment[0].
            logicalLength);

    TEST_ASSERT_TRUE(
        settings.
            ledMappingProfileCustomized());

    TEST_ASSERT_FALSE(
        settings.
            ledMappingProfilePersisted());

    TEST_ASSERT_FALSE(
        Preferences::testHasKey(
            "led_map"));

    TEST_ASSERT_FALSE(
        Preferences::testHasKey(
            "led_map_ver"));
}

void test_topology_version_write_failure_does_not_apply_candidate_live() {
    RuntimeSettings settings;
    TEST_ASSERT_TRUE(settings.begin());

    LedMappingProfile previous;
    previous.segment[1].logicalLength = 150;

    TEST_ASSERT_TRUE(
        settings.setLedMappingProfile(
            previous));

    LedMappingProfile candidate =
        previous;

    candidate.segment[1].logicalLength =
        140;

    Preferences::testFailPut(
        "led_map_ver");

    TEST_ASSERT_FALSE(
        settings.setLedMappingProfile(
            candidate));

    TEST_ASSERT_EQUAL_UINT16(
        150,
        settings.
            ledMappingProfile().
            segment[1].
            logicalLength);

    TEST_ASSERT_TRUE(
        settings.
            ledMappingProfileCustomized());

    TEST_ASSERT_FALSE(
        settings.
            ledMappingProfilePersisted());

    TEST_ASSERT_FALSE(
        Preferences::testHasKey(
            "led_map"));

    TEST_ASSERT_FALSE(
        Preferences::testHasKey(
            "led_map_ver"));
}

void test_topology_reset_marker_failure_keeps_live_custom_state() {
    RuntimeSettings settings;
    TEST_ASSERT_TRUE(settings.begin());

    LedMappingProfile custom;
    custom.segment[0].logicalLength = 200;
    custom.segment[1].logicalLength = 150;

    TEST_ASSERT_TRUE(
        settings.setLedMappingProfile(
            custom));

    Preferences::testFailRemove(
        "led_map_ver");

    TEST_ASSERT_FALSE(
        settings.resetLedMappingProfile());

    TEST_ASSERT_TRUE(
        settings.
            ledMappingProfileCustomized());

    TEST_ASSERT_TRUE(
        settings.
            ledMappingProfilePersisted());

    TEST_ASSERT_EQUAL_UINT16(
        custom.totalLedCount(),
        settings.
            ledMappingProfile().
            totalLedCount());

    TEST_ASSERT_TRUE(
        Preferences::testHasKey(
            "led_map_ver"));
}

void test_topology_reset_data_cleanup_failure_cannot_resurrect_old_blob() {
    RuntimeSettings settings;
    TEST_ASSERT_TRUE(settings.begin());

    LedMappingProfile custom;
    custom.segment[0].logicalLength = 200;

    TEST_ASSERT_TRUE(
        settings.setLedMappingProfile(
            custom));

    Preferences::testFailRemove(
        "led_map");

    TEST_ASSERT_TRUE(
        settings.resetLedMappingProfile());

    TEST_ASSERT_FALSE(
        settings.
            ledMappingProfileCustomized());

    TEST_ASSERT_FALSE(
        settings.
            ledMappingProfilePersisted());

    TEST_ASSERT_FALSE(
        Preferences::testHasKey(
            "led_map_ver"));

    TEST_ASSERT_TRUE(
        Preferences::testHasKey(
            "led_map"));
}

void test_schema1_pixel_mask_is_invalidated_after_physical_index_migration() {
    Preferences legacy;

    TEST_ASSERT_TRUE(
        legacy.begin(
            "ambilight"));

    LedPixelMaskProfile stale;
    stale.disabledOffset[0] = 0;

    TEST_ASSERT_EQUAL_UINT16(
        sizeof(stale),
        legacy.putBytes(
            "pixel_mask",
            &stale,
            sizeof(stale)));

    TEST_ASSERT_EQUAL_UINT16(
        sizeof(std::uint16_t),
        legacy.putUShort(
            "pixel_mask_ver",
            1));

    legacy.end();

    RuntimeSettings settings;

    TEST_ASSERT_TRUE(
        settings.begin());

    TEST_ASSERT_EQUAL_UINT16(
        2,
        LedPixelMaskProfile::
            kSchemaVersion);

    TEST_ASSERT_FALSE(
        settings.
            ledPixelMaskProfileCustomized());

    TEST_ASSERT_FALSE(
        settings.
            ledPixelMaskProfilePersisted());

    TEST_ASSERT_FALSE(
        Preferences::testHasKey(
            "pixel_mask"));

    TEST_ASSERT_FALSE(
        Preferences::testHasKey(
            "pixel_mask_ver"));
}

void test_pixel_mask_reset_marker_failure_keeps_live_mask() {
    RuntimeSettings settings;
    TEST_ASSERT_TRUE(settings.begin());

    LedPixelMaskProfile mask;
    mask.disabledOffset[0] = 7;

    TEST_ASSERT_TRUE(
        settings.setLedPixelMaskProfile(
            mask));

    Preferences::testFailRemove(
        "pixel_mask_ver");

    TEST_ASSERT_FALSE(
        settings.
            resetLedPixelMaskProfile());

    TEST_ASSERT_TRUE(
        settings.
            ledPixelMaskProfileCustomized());

    TEST_ASSERT_EQUAL_UINT16(
        7,
        settings.
            ledPixelMaskProfile().
            disabledOffset[0]);
}

void test_gain_curve_reset_count_failure_keeps_custom_curve() {
    RuntimeSettings settings;
    TEST_ASSERT_TRUE(settings.begin());

    std::array<
        GainPoint,
        ambilight::DistanceGainCurve::
            kMaxPoints>
        points{};

    points[0] = {100, 1024};
    points[1] = {1000, 4096};

    TEST_ASSERT_TRUE(
        settings.setTofGainCurve(
            points,
            2));

    Preferences::testFailRemove(
        "tof_count");

    TEST_ASSERT_FALSE(
        settings.resetTofGainCurve());

    TEST_ASSERT_TRUE(
        settings.
            tofGainCurveCustomized());

    TEST_ASSERT_EQUAL_UINT16(
        1024,
        settings.
            tofGainPoints()[0].
            gainQ12);
}

void test_gain_curve_reset_blob_failure_is_durable_after_marker_removal() {
    RuntimeSettings settings;
    TEST_ASSERT_TRUE(settings.begin());

    std::array<
        GainPoint,
        ambilight::DistanceGainCurve::
            kMaxPoints>
        points{};

    points[0] = {100, 1024};
    points[1] = {1000, 4096};

    TEST_ASSERT_TRUE(
        settings.setTofGainCurve(
            points,
            2));

    Preferences::testFailRemove(
        "tof_curve");

    TEST_ASSERT_TRUE(
        settings.resetTofGainCurve());

    TEST_ASSERT_FALSE(
        settings.
            tofGainCurveCustomized());

    TEST_ASSERT_FALSE(
        Preferences::testHasKey(
            "tof_count"));

    TEST_ASSERT_TRUE(
        Preferences::testHasKey(
            "tof_curve"));
}

void test_spatial_reset_version_failure_keeps_live_profile() {
    RuntimeSettings settings;
    TEST_ASSERT_TRUE(settings.begin());

    TofSpatialProfile profile;
    profile.widthMmX10 = 15000;

    TEST_ASSERT_TRUE(
        profile.valid());

    TEST_ASSERT_TRUE(
        settings.setTofSpatialProfile(
            profile));

    Preferences::testFailRemove(
        "spatial_ver");

    TEST_ASSERT_FALSE(
        settings.resetTofSpatialProfile());

    TEST_ASSERT_TRUE(
        settings.
            tofSpatialProfileCustomized());

    TEST_ASSERT_EQUAL_UINT16(
        15000,
        settings.
            tofSpatialProfile().
            widthMmX10);
}

void test_stage45_brightness_zero_migrates_to_output_off() {
    Preferences legacy;

    TEST_ASSERT_TRUE(
        legacy.begin(
            "ambilight"));

    TEST_ASSERT_EQUAL_UINT16(
        sizeof(std::uint8_t),
        legacy.putUChar(
            "brightness",
            0));

    TEST_ASSERT_FALSE(
        legacy.isKey(
            "output_on"));

    legacy.end();

    RuntimeSettings settings;

    TEST_ASSERT_TRUE(
        settings.begin());

    TEST_ASSERT_FALSE(
        settings.outputEnabled());

    TEST_ASSERT_EQUAL_UINT8(
        0,
        settings.outputBrightness());

    Preferences verify;

    TEST_ASSERT_TRUE(
        verify.begin(
            "ambilight"));

    TEST_ASSERT_EQUAL_UINT8(
        0,
        verify.getUChar(
            "output_on",
            1));

    verify.end();
}

void test_output_power_state_is_independent_and_persists() {
    RuntimeSettings settings;

    TEST_ASSERT_TRUE(
        settings.begin());

    TEST_ASSERT_TRUE(
        settings.outputEnabled());

    TEST_ASSERT_EQUAL_UINT8(
        ambilight::config::
            kDefaultOutputBrightness,
        settings.outputBrightness());

    TEST_ASSERT_TRUE(
        settings.setOutputBrightness(
            77));

    TEST_ASSERT_TRUE(
        settings.setOutputEnabled(
            false));

    TEST_ASSERT_FALSE(
        settings.outputEnabled());

    TEST_ASSERT_EQUAL_UINT8(
        77,
        settings.outputBrightness());

    RuntimeSettings restored;

    TEST_ASSERT_TRUE(
        restored.begin());

    TEST_ASSERT_FALSE(
        restored.outputEnabled());

    TEST_ASSERT_EQUAL_UINT8(
        77,
        restored.outputBrightness());
}

void test_invalid_output_power_value_self_heals_to_on() {
    Preferences legacy;

    TEST_ASSERT_TRUE(
        legacy.begin(
            "ambilight"));

    TEST_ASSERT_EQUAL_UINT16(
        sizeof(std::uint8_t),
        legacy.putUChar(
            "output_on",
            7));

    legacy.end();

    RuntimeSettings settings;

    TEST_ASSERT_TRUE(
        settings.begin());

    TEST_ASSERT_TRUE(
        settings.outputEnabled());

    Preferences verify;

    TEST_ASSERT_TRUE(
        verify.begin(
            "ambilight"));

    TEST_ASSERT_EQUAL_UINT8(
        1,
        verify.getUChar(
            "output_on",
            0));

    verify.end();
}

void test_wifi_clear_ssid_failure_keeps_live_credentials() {
    RuntimeSettings settings;
    TEST_ASSERT_TRUE(settings.begin());

    TEST_ASSERT_TRUE(
        settings.setWifiCredentials(
            "ambilight-test",
            "secret"));

    Preferences::testFailRemove(
        "wifi_ssid");

    TEST_ASSERT_FALSE(
        settings.clearWifiCredentials());

    TEST_ASSERT_TRUE(
        settings.
            wifiCredentialsPresent());

    TEST_ASSERT_EQUAL_STRING(
        "ambilight-test",
        settings.wifiSsid());
}

void test_factory_reset_failure_leaves_live_runtime_untouched() {
    RuntimeSettings settings;
    TEST_ASSERT_TRUE(settings.begin());

    TEST_ASSERT_TRUE(
        settings.setOutputBrightness(
            77));

    Preferences::testFailClear(
        true);

    TEST_ASSERT_FALSE(
        settings.factoryReset());

    TEST_ASSERT_EQUAL_UINT8(
        77,
        settings.outputBrightness());

    TEST_ASSERT_TRUE(
        settings.setOutputEnabled(
            false));

    TEST_ASSERT_FALSE(
        settings.factoryReset());

    // Failed durable reset must not mutate the live power state either.
    TEST_ASSERT_FALSE(
        settings.outputEnabled());
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(
        test_schema2_topology_is_invalidated_and_measured_default_wins);

    RUN_TEST(
        test_schema3_custom_topology_survives_reboot);

    RUN_TEST(
        test_topology_blob_write_failure_does_not_apply_candidate_live);

    RUN_TEST(
        test_topology_version_write_failure_does_not_apply_candidate_live);

    RUN_TEST(
        test_topology_reset_marker_failure_keeps_live_custom_state);

    RUN_TEST(
        test_topology_reset_data_cleanup_failure_cannot_resurrect_old_blob);

    RUN_TEST(
        test_schema1_pixel_mask_is_invalidated_after_physical_index_migration);

    RUN_TEST(
        test_pixel_mask_reset_marker_failure_keeps_live_mask);

    RUN_TEST(
        test_gain_curve_reset_count_failure_keeps_custom_curve);

    RUN_TEST(
        test_gain_curve_reset_blob_failure_is_durable_after_marker_removal);

    RUN_TEST(
        test_spatial_reset_version_failure_keeps_live_profile);

    RUN_TEST(
        test_stage45_brightness_zero_migrates_to_output_off);

    RUN_TEST(
        test_output_power_state_is_independent_and_persists);

    RUN_TEST(
        test_invalid_output_power_value_self_heals_to_on);

    RUN_TEST(
        test_wifi_clear_ssid_failure_keeps_live_credentials);

    RUN_TEST(
        test_factory_reset_failure_leaves_live_runtime_untouched);

    return UNITY_END();
}
