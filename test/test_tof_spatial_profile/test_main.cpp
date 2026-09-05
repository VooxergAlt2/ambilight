#include <unity.h>

#include "tof/TofSpatialProfile.h"

using ambilight::SegmentId;
using ambilight::TofRotation;
using ambilight::TofSpatialProfile;

void test_default_profile_matches_current_led_geometry() {
    TofSpatialProfile profile;

    TEST_ASSERT_TRUE(
        profile.valid());

    TEST_ASSERT_FLOAT_WITHIN(
        0.01F,
        1437.5F,
        profile.widthMm());

    TEST_ASSERT_FLOAT_WITHIN(
        0.01F,
        1000.0F,
        profile.heightMm());

    TEST_ASSERT_FLOAT_WITHIN(
        0.01F,
        10.0F,
        profile.planeDeadbandMm());

    const auto geometry =
        profile.perimeterGeometry();

    const auto& top =
        geometry[
            static_cast<std::size_t>(
                SegmentId::Top)];

    TEST_ASSERT_FLOAT_WITHIN(
        0.01F,
        -718.75F,
        top.logicalStart.xMm);

    TEST_ASSERT_FLOAT_WITHIN(
        0.01F,
        500.0F,
        top.logicalStart.yMm);

    TEST_ASSERT_FLOAT_WITHIN(
        0.01F,
        718.75F,
        top.logicalEnd.xMm);
}

void test_sensor_offsets_shift_all_led_coordinates_relative_to_tof() {
    TofSpatialProfile profile;
    profile.widthMmX10 = 10000;
    profile.heightMmX10 = 5000;
    profile.sensorOffsetXmmX10 = 1000;
    profile.sensorOffsetYmmX10 = -500;
    profile.ledPlaneZmmX10 = 200;

    const auto geometry =
        profile.perimeterGeometry();

    const auto& top =
        geometry[
            static_cast<std::size_t>(
                SegmentId::Top)];

    // Screen top-left is (-500,+250). Sensor is (+100,-50) from screen
    // centre, so coordinates relative to the sensor are (-600,+300).
    TEST_ASSERT_FLOAT_WITHIN(
        0.01F,
        -600.0F,
        top.logicalStart.xMm);

    TEST_ASSERT_FLOAT_WITHIN(
        0.01F,
        300.0F,
        top.logicalStart.yMm);

    TEST_ASSERT_FLOAT_WITHIN(
        0.01F,
        20.0F,
        top.logicalStart.zMm);
}

void test_orientation_maps_to_grid_transform() {
    TofSpatialProfile profile;

    profile.rotationQuarterTurns = 3;
    profile.mirrorX = 1;

    const auto transform =
        profile.transform();

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            TofRotation::Deg270),
        static_cast<int>(
            transform.rotation));

    TEST_ASSERT_TRUE(
        transform.mirrorX);
}

void test_invalid_profile_bounds_are_rejected() {
    TofSpatialProfile profile;

    profile.widthMmX10 = 0;
    TEST_ASSERT_FALSE(
        profile.valid());

    profile = {};
    profile.rotationQuarterTurns = 4;
    TEST_ASSERT_FALSE(
        profile.valid());

    profile = {};
    profile.mirrorX = 2;
    TEST_ASSERT_FALSE(
        profile.valid());

    profile = {};
    profile.planeDeadbandMmX10 = 0;
    TEST_ASSERT_FALSE(
        profile.valid());
}

void test_plane_gate_config_uses_runtime_geometry_and_deadband() {
    TofSpatialProfile profile;
    profile.widthMmX10 = 10000;
    profile.heightMmX10 = 5000;
    profile.planeDeadbandMmX10 = 125;

    const auto gate =
        profile.planeGateConfig();

    TEST_ASSERT_FLOAT_WITHIN(
        0.01F,
        12.5F,
        gate.wallDeltaDeadbandMm);

    const auto& right =
        gate.geometry[
            static_cast<std::size_t>(
                SegmentId::Right)];

    TEST_ASSERT_FLOAT_WITHIN(
        0.01F,
        500.0F,
        right.logicalStart.xMm);

    TEST_ASSERT_FLOAT_WITHIN(
        0.01F,
        250.0F,
        right.logicalStart.yMm);

    TEST_ASSERT_FLOAT_WITHIN(
        0.01F,
        -250.0F,
        right.logicalEnd.yMm);
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_default_profile_matches_current_led_geometry);
    RUN_TEST(test_sensor_offsets_shift_all_led_coordinates_relative_to_tof);
    RUN_TEST(test_orientation_maps_to_grid_transform);
    RUN_TEST(test_invalid_profile_bounds_are_rejected);
    RUN_TEST(test_plane_gate_config_uses_runtime_geometry_and_deadband);

    return UNITY_END();
}
