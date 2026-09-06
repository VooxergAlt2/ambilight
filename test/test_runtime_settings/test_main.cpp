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
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(
        test_topology_reset_marker_failure_keeps_live_custom_state);

    RUN_TEST(
        test_topology_reset_data_cleanup_failure_cannot_resurrect_old_blob);

    RUN_TEST(
        test_pixel_mask_reset_marker_failure_keeps_live_mask);

    RUN_TEST(
        test_gain_curve_reset_count_failure_keeps_custom_curve);

    RUN_TEST(
        test_gain_curve_reset_blob_failure_is_durable_after_marker_removal);

    RUN_TEST(
        test_spatial_reset_version_failure_keeps_live_profile);

    RUN_TEST(
        test_wifi_clear_ssid_failure_keeps_live_credentials);

    RUN_TEST(
        test_factory_reset_failure_leaves_live_runtime_untouched);

    return UNITY_END();
}
