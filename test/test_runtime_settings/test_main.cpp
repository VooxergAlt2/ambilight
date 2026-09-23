#include <unity.h>

#include <Preferences.h>

#include "config/BoardConfig.h"
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

void test_schema2_topology_is_ignored_but_preserved_for_recovery() {
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

    TEST_ASSERT_TRUE(
        Preferences::testHasKey(
            "led_map"));

    TEST_ASSERT_TRUE(
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


void test_unknown_output_state_schema_is_preserved_without_legacy_overwrite() {
    Preferences stored;
    TEST_ASSERT_TRUE(stored.begin("ambilight"));

    const std::uint8_t futureRecord[4] = {99, 0, 77, 0};

    TEST_ASSERT_EQUAL_UINT16(
        sizeof(futureRecord),
        stored.putBytes(
            "output_state",
            futureRecord,
            sizeof(futureRecord)));

    TEST_ASSERT_EQUAL_UINT16(
        sizeof(std::uint8_t),
        stored.putUChar(
            "brightness",
            11));

    stored.end();

    RuntimeSettings settings;
    TEST_ASSERT_TRUE(settings.begin());

    TEST_ASSERT_TRUE(settings.outputEnabled());
    TEST_ASSERT_EQUAL_UINT8(
        ambilight::config::kDefaultOutputBrightness,
        settings.outputBrightness());

    TEST_ASSERT_TRUE(
        Preferences::testHasKey(
            "output_state"));

    TEST_ASSERT_TRUE(
        Preferences::testHasKey(
            "brightness"));
}

void test_invalid_correction_value_is_preserved_for_recovery() {
    Preferences stored;
    TEST_ASSERT_TRUE(stored.begin("ambilight"));

    TEST_ASSERT_EQUAL_UINT16(
        sizeof(std::uint8_t),
        stored.putUChar(
            "corr_mode",
            99));

    stored.end();

    RuntimeSettings settings;
    TEST_ASSERT_TRUE(settings.begin());

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(
            ambilight::CorrectionMode::Shadow),
        static_cast<std::uint8_t>(
            settings.correctionMode()));

    Preferences verify;
    TEST_ASSERT_TRUE(verify.begin("ambilight"));
    TEST_ASSERT_EQUAL_UINT8(
        99,
        verify.getUChar(
            "corr_mode",
            0));
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
        settings.ledMappingProfileCustomized());
    TEST_ASSERT_TRUE(
        settings.ledMappingProfilePersisted());
    TEST_ASSERT_TRUE(Preferences::testHasKey("led_map"));
    TEST_ASSERT_TRUE(Preferences::testHasKey("led_map_ver"));

    RuntimeSettings reopened;
    TEST_ASSERT_TRUE(reopened.begin());
    TEST_ASSERT_EQUAL_UINT16(
        220,
        reopened.ledMappingProfile().segment[0].logicalLength);
}

void test_first_topology_version_write_failure_does_not_commit_candidate() {
    RuntimeSettings settings;
    TEST_ASSERT_TRUE(settings.begin());

    LedMappingProfile candidate;
    candidate.segment[1].logicalLength = 140;

    Preferences::testFailPut("led_map_ver");

    TEST_ASSERT_FALSE(
        settings.setLedMappingProfile(candidate));

    TEST_ASSERT_FALSE(settings.ledMappingProfileCustomized());
    TEST_ASSERT_FALSE(settings.ledMappingProfilePersisted());
    TEST_ASSERT_FALSE(Preferences::testHasKey("led_map"));
    TEST_ASSERT_FALSE(Preferences::testHasKey("led_map_ver"));
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

void test_schema1_pixel_mask_is_ignored_but_preserved_for_recovery() {
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

    TEST_ASSERT_TRUE(
        Preferences::testHasKey(
            "pixel_mask"));

    TEST_ASSERT_TRUE(
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


void test_pixel_mask_update_blob_failure_keeps_previous_live_and_durable_profile() {
    RuntimeSettings settings;
    TEST_ASSERT_TRUE(settings.begin());

    LedPixelMaskProfile previous;
    previous.disabledOffset[0] = 3;
    TEST_ASSERT_TRUE(
        settings.setLedPixelMaskProfile(
            previous));

    LedPixelMaskProfile candidate = previous;
    candidate.disabledOffset[0] = 7;

    Preferences::testFailPut(
        "pixel_mask");

    TEST_ASSERT_FALSE(
        settings.setLedPixelMaskProfile(
            candidate));

    TEST_ASSERT_EQUAL_UINT16(
        3,
        settings.ledPixelMaskProfile().disabledOffset[0]);
    TEST_ASSERT_TRUE(
        settings.ledPixelMaskProfilePersisted());

    RuntimeSettings reopened;
    TEST_ASSERT_TRUE(reopened.begin());
    TEST_ASSERT_EQUAL_UINT16(
        3,
        reopened.ledPixelMaskProfile().disabledOffset[0]);
}

void test_first_pixel_mask_version_failure_does_not_commit_live_or_orphan_blob() {
    RuntimeSettings settings;
    TEST_ASSERT_TRUE(settings.begin());

    LedPixelMaskProfile candidate;
    candidate.disabledOffset[0] = 0;

    Preferences::testFailPut(
        "pixel_mask_ver");

    TEST_ASSERT_FALSE(
        settings.setLedPixelMaskProfile(
            candidate));

    TEST_ASSERT_EQUAL_UINT16(
        LedPixelMaskProfile::kNone,
        settings.ledPixelMaskProfile().disabledOffset[0]);
    TEST_ASSERT_FALSE(
        settings.ledPixelMaskProfilePersisted());
    TEST_ASSERT_FALSE(
        Preferences::testHasKey("pixel_mask"));
    TEST_ASSERT_FALSE(
        Preferences::testHasKey("pixel_mask_ver"));
}

void test_schema2_pixel_mask_offset_is_preserved_with_physical_hole_semantics() {
    {
        RuntimeSettings settings;
        TEST_ASSERT_TRUE(settings.begin());

        LedPixelMaskProfile mask;
        mask.disabledOffset[0] = 0;
        TEST_ASSERT_TRUE(
            settings.setLedPixelMaskProfile(
                mask));
    }

    RuntimeSettings reopened;
    TEST_ASSERT_TRUE(reopened.begin());
    TEST_ASSERT_EQUAL_UINT16(
        2,
        LedPixelMaskProfile::kSchemaVersion);
    TEST_ASSERT_EQUAL_UINT16(
        0,
        reopened.ledPixelMaskProfile().disabledOffset[0]);
    TEST_ASSERT_TRUE(
        reopened.ledPixelMaskProfile().validFor(
            reopened.ledMappingProfile()));
    TEST_ASSERT_EQUAL_UINT32(
        231,
        reopened.ledPixelMaskProfile().maxPhysicalLaneLength(
            reopened.ledMappingProfile()));
}


void test_runtime_only_mask_adoption_tracks_already_applied_safe_geometry() {
    RuntimeSettings settings;
    TEST_ASSERT_TRUE(settings.begin());

    LedPixelMaskProfile runtimeOnly;
    runtimeOnly.disabledOffset[0] = 0;

    TEST_ASSERT_TRUE(
        settings.adoptLedPixelMaskProfileRuntime(
            runtimeOnly));

    TEST_ASSERT_EQUAL_UINT16(
        0,
        settings.ledPixelMaskProfile().disabledOffset[0]);
    TEST_ASSERT_TRUE(settings.ledPixelMaskProfileCustomized());
    TEST_ASSERT_FALSE(settings.ledPixelMaskProfilePersisted());
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

    TEST_ASSERT_TRUE(
        Preferences::testHasKey(
            "output_state"));

    TEST_ASSERT_FALSE(
        Preferences::testHasKey(
            "brightness"));

    TEST_ASSERT_FALSE(
        Preferences::testHasKey(
            "output_on"));
}

void test_early_stage46_split_output_keys_migrate_atomically() {
    Preferences legacy;

    TEST_ASSERT_TRUE(
        legacy.begin(
            "ambilight"));

    TEST_ASSERT_EQUAL_UINT16(
        sizeof(std::uint8_t),
        legacy.putUChar(
            "brightness",
            77));

    TEST_ASSERT_EQUAL_UINT16(
        sizeof(std::uint8_t),
        legacy.putUChar(
            "output_on",
            0));

    legacy.end();

    RuntimeSettings settings;

    TEST_ASSERT_TRUE(
        settings.begin());

    TEST_ASSERT_FALSE(
        settings.outputEnabled());

    TEST_ASSERT_EQUAL_UINT8(
        77,
        settings.outputBrightness());

    TEST_ASSERT_TRUE(
        Preferences::testHasKey(
            "output_state"));

    TEST_ASSERT_FALSE(
        Preferences::testHasKey(
            "brightness"));

    TEST_ASSERT_FALSE(
        Preferences::testHasKey(
            "output_on"));
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

    TEST_ASSERT_TRUE(
        Preferences::testHasKey(
            "output_state"));

    TEST_ASSERT_FALSE(
        Preferences::testHasKey(
            "output_on"));
}

void test_output_state_write_failure_cannot_persist_half_an_update() {
    {
        RuntimeSettings settings;

        TEST_ASSERT_TRUE(
            settings.begin());

        Preferences::testFailPut(
            "output_state");

        TEST_ASSERT_FALSE(
            settings.setOutputState(
                false,
                77));

        // Runtime control stays available even though persistence failed.
        TEST_ASSERT_FALSE(
            settings.outputEnabled());

        TEST_ASSERT_EQUAL_UINT8(
            77,
            settings.outputBrightness());
    }

    Preferences::testFailPut(
        nullptr);

    RuntimeSettings restored;

    TEST_ASSERT_TRUE(
        restored.begin());

    // The previous complete record wins. We never persist only brightness or
    // only power.
    TEST_ASSERT_TRUE(
        restored.outputEnabled());

    TEST_ASSERT_EQUAL_UINT8(
        ambilight::config::
            kDefaultOutputBrightness,
        restored.outputBrightness());
}

void test_identical_output_state_retries_failed_persistence() {
    RuntimeSettings settings;

    TEST_ASSERT_TRUE(
        settings.begin());

    Preferences::testFailPut(
        "output_state");

    TEST_ASSERT_FALSE(
        settings.setOutputState(
            false,
            77));

    Preferences::testFailPut(
        nullptr);

    // The live values are unchanged, but the failed durable write must be
    // retried rather than short-circuited as an already persisted state.
    TEST_ASSERT_TRUE(
        settings.setOutputState(
            false,
            77));

    RuntimeSettings restored;

    TEST_ASSERT_TRUE(
        restored.begin());

    TEST_ASSERT_FALSE(
        restored.outputEnabled());

    TEST_ASSERT_EQUAL_UINT8(
        77,
        restored.outputBrightness());
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


void test_complete_current_configuration_survives_reopen() {
    LedMappingProfile mapping;
    mapping.segment[0].logicalLength = 220;
    mapping.segment[1].logicalLength = 150;

    LedPixelMaskProfile mask;
    mask.disabledOffset[0] = 3;
    mask.disabledOffset[1] = 7;

    TofSpatialProfile spatial;
    spatial.widthMmX10 = 15000;
    spatial.heightMmX10 = 9000;
    spatial.sensorOffsetXmmX10 = 120;
    spatial.sensorOffsetYmmX10 = -80;
    spatial.ledPlaneZmmX10 = 35;
    spatial.rotationQuarterTurns = 1;
    spatial.mirrorX = 1;
    spatial.planeDeadbandMmX10 = 125;

    std::array<
        GainPoint,
        ambilight::DistanceGainCurve::kMaxPoints>
        curve{};

    curve[0] = {50, 3000};
    curve[1] = {4000, 4096};

    {
        RuntimeSettings settings;
        TEST_ASSERT_TRUE(settings.begin());
        TEST_ASSERT_TRUE(
            settings.setCorrectionMode(
                ambilight::CorrectionMode::Active));
        TEST_ASSERT_TRUE(
            settings.setOutputState(false, 77));
        TEST_ASSERT_TRUE(
            settings.setWifiCredentials(
                "upgrade-test",
                "upgrade-secret"));
        TEST_ASSERT_TRUE(
            settings.setLedMappingProfile(mapping));
        TEST_ASSERT_TRUE(
            settings.setLedPixelMaskProfile(mask));
        TEST_ASSERT_TRUE(
            settings.setTofSpatialProfile(spatial));
        TEST_ASSERT_TRUE(
            settings.setTofGainCurve(curve, 2));
    }

    RuntimeSettings restored;
    TEST_ASSERT_TRUE(restored.begin());

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(
            ambilight::CorrectionMode::Active),
        static_cast<std::uint8_t>(
            restored.correctionMode()));
    TEST_ASSERT_FALSE(restored.outputEnabled());
    TEST_ASSERT_EQUAL_UINT8(77, restored.outputBrightness());
    TEST_ASSERT_EQUAL_STRING("upgrade-test", restored.wifiSsid());
    TEST_ASSERT_EQUAL_STRING("upgrade-secret", restored.wifiPassword());
    TEST_ASSERT_EQUAL_UINT16(
        220,
        restored.ledMappingProfile().segment[0].logicalLength);
    TEST_ASSERT_EQUAL_UINT16(
        150,
        restored.ledMappingProfile().segment[1].logicalLength);
    TEST_ASSERT_EQUAL_UINT16(
        3,
        restored.ledPixelMaskProfile().disabledOffset[0]);
    TEST_ASSERT_EQUAL_UINT16(
        7,
        restored.ledPixelMaskProfile().disabledOffset[1]);
    TEST_ASSERT_EQUAL_UINT16(
        15000,
        restored.tofSpatialProfile().widthMmX10);
    TEST_ASSERT_EQUAL_UINT16(
        9000,
        restored.tofSpatialProfile().heightMmX10);
    TEST_ASSERT_EQUAL_INT16(
        120,
        restored.tofSpatialProfile().sensorOffsetXmmX10);
    TEST_ASSERT_EQUAL_INT16(
        -80,
        restored.tofSpatialProfile().sensorOffsetYmmX10);
    TEST_ASSERT_EQUAL_INT16(
        35,
        restored.tofSpatialProfile().ledPlaneZmmX10);
    TEST_ASSERT_EQUAL_UINT8(
        1,
        restored.tofSpatialProfile().rotationQuarterTurns);
    TEST_ASSERT_EQUAL_UINT8(
        1,
        restored.tofSpatialProfile().mirrorX);
    TEST_ASSERT_EQUAL_UINT16(
        125,
        restored.tofSpatialProfile().planeDeadbandMmX10);
    TEST_ASSERT_EQUAL_UINT16(2, restored.tofGainPointCount());
    TEST_ASSERT_EQUAL_UINT16(
        50,
        restored.tofGainPoints()[0].distanceMm);
    TEST_ASSERT_EQUAL_UINT16(
        3000,
        restored.tofGainPoints()[0].gainQ12);
    TEST_ASSERT_EQUAL_UINT16(
        4000,
        restored.tofGainPoints()[1].distanceMm);
    TEST_ASSERT_EQUAL_UINT16(
        4096,
        restored.tofGainPoints()[1].gainQ12);
}


void test_output_state_v1_migrates_brightness_into_both_banks() {
    struct LegacyOutputStateRecord {
        std::uint16_t schemaVersion;
        std::uint8_t brightness;
        std::uint8_t enabled;
    };
    static_assert(sizeof(LegacyOutputStateRecord) == 4);

    Preferences stored;
    TEST_ASSERT_TRUE(stored.begin("ambilight"));

    const LegacyOutputStateRecord legacy{1, 77, 1};
    TEST_ASSERT_EQUAL_UINT16(
        sizeof(legacy),
        stored.putBytes(
            "output_state",
            &legacy,
            sizeof(legacy)));
    stored.end();

    RuntimeSettings settings;
    TEST_ASSERT_TRUE(settings.begin());
    TEST_ASSERT_TRUE(settings.outputEnabled());
    TEST_ASSERT_EQUAL_UINT8(77, settings.ddpBrightness());
    TEST_ASSERT_EQUAL_UINT8(77, settings.lightingBrightness());

    Preferences verify;
    TEST_ASSERT_TRUE(verify.begin("ambilight"));
    TEST_ASSERT_EQUAL_UINT16(
        6,
        verify.getBytesLength("output_state"));
}

void test_split_brightness_banks_survive_reopen() {
    {
        RuntimeSettings settings;
        TEST_ASSERT_TRUE(settings.begin());
        TEST_ASSERT_TRUE(
            settings.setOutputState(
                true,
                210,
                43));
        TEST_ASSERT_EQUAL_UINT8(210, settings.ddpBrightness());
        TEST_ASSERT_EQUAL_UINT8(43, settings.lightingBrightness());
    }

    RuntimeSettings restored;
    TEST_ASSERT_TRUE(restored.begin());
    TEST_ASSERT_TRUE(restored.outputEnabled());
    TEST_ASSERT_EQUAL_UINT8(210, restored.ddpBrightness());
    TEST_ASSERT_EQUAL_UINT8(43, restored.lightingBrightness());
}

void test_legacy_brightness_setter_updates_both_banks_atomically() {
    RuntimeSettings settings;
    TEST_ASSERT_TRUE(settings.begin());
    TEST_ASSERT_TRUE(settings.setOutputBrightness(91));
    TEST_ASSERT_EQUAL_UINT8(91, settings.ddpBrightness());
    TEST_ASSERT_EQUAL_UINT8(91, settings.lightingBrightness());
}

void test_manual_lighting_state_survives_reopen() {
    {
        RuntimeSettings settings;
        TEST_ASSERT_TRUE(settings.begin());

        ambilight::ManualLightingState state;
        state.effect = ambilight::ManualLightingEffect::Aurora;
        state.fallbackEffect = ambilight::ManualLightingEffect::Aurora;
        state.color = {12, 34, 56};
        state.speed = 201;
        state.intensity = 77;

        TEST_ASSERT_TRUE(
            settings.setManualLightingState(
                state));
    }

    RuntimeSettings restored;
    TEST_ASSERT_TRUE(restored.begin());
    const auto& state = restored.manualLightingState();
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(
            ambilight::ManualLightingEffect::Aurora),
        static_cast<std::uint8_t>(state.effect));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(
            ambilight::ManualLightingEffect::Aurora),
        static_cast<std::uint8_t>(state.fallbackEffect));
    TEST_ASSERT_EQUAL_UINT8(12, state.color.r);
    TEST_ASSERT_EQUAL_UINT8(34, state.color.g);
    TEST_ASSERT_EQUAL_UINT8(56, state.color.b);
    TEST_ASSERT_EQUAL_UINT8(201, state.speed);
    TEST_ASSERT_EQUAL_UINT8(77, state.intensity);
}

void test_future_manual_lighting_record_is_preserved() {
    Preferences stored;
    TEST_ASSERT_TRUE(stored.begin("ambilight"));

    const std::uint8_t futureRecord[10] = {
        99, 0, 8, 8, 1, 2, 3, 4, 5, 0
    };

    TEST_ASSERT_EQUAL_UINT16(
        sizeof(futureRecord),
        stored.putBytes(
            "manual_light",
            futureRecord,
            sizeof(futureRecord)));
    stored.end();

    RuntimeSettings settings;
    TEST_ASSERT_TRUE(settings.begin());

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(
            ambilight::ManualLightingEffect::Ambilight),
        static_cast<std::uint8_t>(
            settings.manualLightingState().effect));

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(
            ambilight::ManualLightingEffect::BiasWhite),
        static_cast<std::uint8_t>(
            settings.manualLightingState().fallbackEffect));

    Preferences verify;
    TEST_ASSERT_TRUE(verify.begin("ambilight"));
    TEST_ASSERT_EQUAL_UINT16(
        sizeof(futureRecord),
        verify.getBytesLength("manual_light"));
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(
        test_schema2_topology_is_ignored_but_preserved_for_recovery);

    RUN_TEST(
        test_unknown_output_state_schema_is_preserved_without_legacy_overwrite);

    RUN_TEST(
        test_invalid_correction_value_is_preserved_for_recovery);

    RUN_TEST(
        test_schema3_custom_topology_survives_reboot);

    RUN_TEST(
        test_topology_blob_write_failure_does_not_apply_candidate_live);

    RUN_TEST(
        test_first_topology_version_write_failure_does_not_commit_candidate);

    RUN_TEST(
        test_topology_reset_marker_failure_keeps_live_custom_state);

    RUN_TEST(
        test_topology_reset_data_cleanup_failure_cannot_resurrect_old_blob);

    RUN_TEST(
        test_schema1_pixel_mask_is_ignored_but_preserved_for_recovery);

    RUN_TEST(
        test_pixel_mask_reset_marker_failure_keeps_live_mask);
    RUN_TEST(test_pixel_mask_update_blob_failure_keeps_previous_live_and_durable_profile);
    RUN_TEST(test_first_pixel_mask_version_failure_does_not_commit_live_or_orphan_blob);
    RUN_TEST(test_schema2_pixel_mask_offset_is_preserved_with_physical_hole_semantics);
    RUN_TEST(test_runtime_only_mask_adoption_tracks_already_applied_safe_geometry);

    RUN_TEST(
        test_gain_curve_reset_count_failure_keeps_custom_curve);

    RUN_TEST(
        test_gain_curve_reset_blob_failure_is_durable_after_marker_removal);

    RUN_TEST(
        test_spatial_reset_version_failure_keeps_live_profile);

    RUN_TEST(
        test_stage45_brightness_zero_migrates_to_output_off);

    RUN_TEST(
        test_early_stage46_split_output_keys_migrate_atomically);

    RUN_TEST(
        test_output_power_state_is_independent_and_persists);

    RUN_TEST(
        test_invalid_output_power_value_self_heals_to_on);

    RUN_TEST(
        test_output_state_write_failure_cannot_persist_half_an_update);

    RUN_TEST(
        test_identical_output_state_retries_failed_persistence);

    RUN_TEST(test_output_state_v1_migrates_brightness_into_both_banks);
    RUN_TEST(test_split_brightness_banks_survive_reopen);
    RUN_TEST(test_legacy_brightness_setter_updates_both_banks_atomically);
    RUN_TEST(test_manual_lighting_state_survives_reopen);
    RUN_TEST(test_future_manual_lighting_record_is_preserved);

    RUN_TEST(
        test_wifi_clear_ssid_failure_keeps_live_credentials);

    RUN_TEST(
        test_factory_reset_failure_leaves_live_runtime_untouched);

    RUN_TEST(
        test_complete_current_configuration_survives_reopen);

    return UNITY_END();
}
