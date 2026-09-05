#include <array>
#include <cmath>
#include <cstdint>

#include <unity.h>

#include "tof/TofGrid.h"
#include "tof/TofPlaneEstimator.h"

using ambilight::TofGridTransform;
using ambilight::TofPlaneEstimator;
using ambilight::TofRawFrame;

namespace {

constexpr double kPi = 3.14159265358979323846;

// Independent sensor-contract fixture from ST's published 8x8 zone-center LUT.
constexpr std::array<double, 64> kPitchDeg = {{
    62.85, 66.50, 69.40, 71.08, 71.08, 69.40, 66.50, 62.85,
    66.50, 70.81, 75.05, 77.50, 77.50, 75.05, 70.81, 66.50,
    69.40, 75.05, 78.15, 81.76, 81.76, 78.15, 75.05, 69.40,
    71.08, 77.50, 81.76, 86.00, 86.00, 81.76, 77.50, 71.08,
    71.08, 77.50, 81.76, 86.00, 86.00, 81.76, 77.50, 71.08,
    69.40, 75.05, 78.15, 81.76, 81.76, 78.15, 75.05, 69.40,
    66.50, 70.81, 75.05, 77.50, 77.50, 75.05, 70.81, 66.50,
    62.85, 66.50, 69.40, 71.08, 71.08, 69.40, 66.50, 62.85
}};

constexpr std::array<double, 64> kYawDeg = {{
    135.00, 125.40, 113.20, 98.13, 81.87, 66.80, 54.60, 45.00,
    144.60, 135.00, 120.96, 101.31, 78.69, 59.04, 45.00, 35.40,
    156.80, 149.04, 135.00, 108.45, 71.55, 45.00, 30.96, 23.20,
    171.87, 168.69, 161.55, 135.00, 45.00, 18.45, 11.31, 8.13,
    188.13, 191.31, 198.45, 225.00, 315.00, 341.55, 348.69, 351.87,
    203.20, 210.96, 225.00, 251.55, 288.45, 315.00, 329.04, 336.80,
    203.20, 225.00, 239.04, 258.69, 281.31, 300.96, 315.00, 324.60,
    225.00, 234.60, 246.80, 261.87, 278.13, 293.20, 305.40, 315.00
}};

double degToRad(double degrees) {
    return degrees * kPi / 180.0;
}

TofRawFrame makeFlatWall(
    std::uint16_t distanceMm,
    std::uint64_t timestampUs,
    const TofGridTransform& transform = {},
    std::uint8_t status = 5) {

    TofRawFrame frame;
    frame.timestampUs = timestampUs;

    for (std::size_t row = 0; row < 8; ++row) {
        for (std::size_t col = 0; col < 8; ++col) {
            const std::size_t rawIndex =
                ambilight::tofRawIndexForNormalized(
                    row,
                    col,
                    transform);

            // Core VL53L5CX contract: a perpendicular flat wall produces the
            // same perpendicular distance in all zones.
            frame.distanceMm[rawIndex] =
                static_cast<std::int16_t>(distanceMm);

            frame.targetStatus[rawIndex] =
                status;
        }
    }

    return frame;
}

TofRawFrame makePlane(
    double interceptMm,
    double slopeX,
    double slopeY,
    std::uint64_t timestampUs,
    const TofGridTransform& transform = {},
    std::uint8_t status = 5) {

    TofRawFrame frame;
    frame.timestampUs = timestampUs;

    for (std::size_t row = 0; row < 8; ++row) {
        for (std::size_t col = 0; col < 8; ++col) {
            const std::size_t normalizedIndex =
                row * 8 + col;

            const double pitch =
                degToRad(kPitchDeg[normalizedIndex]);

            const double yaw =
                degToRad(kYawDeg[normalizedIndex]);

            // From ST point-cloud equations:
            // x/z = cos(yaw)*cos(pitch)/sin(pitch)
            // y/z = sin(yaw)*cos(pitch)/sin(pitch)
            const double cotPitch =
                std::cos(pitch) /
                std::sin(pitch);

            const double xOverZ =
                std::cos(yaw) * cotPitch;

            const double yOverZ =
                std::sin(yaw) * cotPitch;

            // z = intercept + slopeX*x + slopeY*y
            const double denominator =
                1.0 -
                slopeX * xOverZ -
                slopeY * yOverZ;

            const double zMm =
                interceptMm / denominator;

            const std::size_t rawIndex =
                ambilight::tofRawIndexForNormalized(
                    row,
                    col,
                    transform);

            frame.distanceMm[rawIndex] =
                static_cast<std::int16_t>(
                    std::lround(zMm));

            frame.targetStatus[rawIndex] =
                status;
        }
    }

    return frame;
}

} // namespace

void test_flat_wall_uses_perpendicular_distance_contract() {
    TofPlaneEstimator estimator;

    const auto plane =
        estimator.estimate(
            makeFlatWall(
                800,
                1000000),
            {});

    TEST_ASSERT_TRUE(plane.valid);
    TEST_ASSERT_EQUAL_UINT8(64, plane.candidates);
    TEST_ASSERT_TRUE(plane.accepted >= 60);

    TEST_ASSERT_FLOAT_WITHIN(
        1.0F,
        800.0F,
        plane.interceptMm);

    TEST_ASSERT_FLOAT_WITHIN(
        0.002F,
        0.0F,
        plane.slopeX);

    TEST_ASSERT_FLOAT_WITHIN(
        0.002F,
        0.0F,
        plane.slopeY);

    TEST_ASSERT_INT_WITHIN(
        15,
        0,
        plane.yawCentiDeg);

    TEST_ASSERT_INT_WITHIN(
        15,
        0,
        plane.pitchCentiDeg);
}

void test_combined_yaw_and_pitch_are_recovered() {
    TofPlaneEstimator estimator;

    const auto plane =
        estimator.estimate(
            makePlane(
                900.0,
                0.20,
                -0.12,
                1000000),
            {});

    TEST_ASSERT_TRUE(plane.valid);

    TEST_ASSERT_FLOAT_WITHIN(
        3.0F,
        900.0F,
        plane.interceptMm);

    TEST_ASSERT_FLOAT_WITHIN(
        0.015F,
        0.20F,
        plane.slopeX);

    TEST_ASSERT_FLOAT_WITHIN(
        0.015F,
        -0.12F,
        plane.slopeY);

    TEST_ASSERT_TRUE(
        plane.yawCentiDeg > 1000);

    TEST_ASSERT_TRUE(
        plane.pitchCentiDeg < -500);
}

void test_extreme_outliers_are_rejected() {
    TofPlaneEstimator estimator;

    auto frame =
        makePlane(
            750.0,
            0.12,
            0.08,
            1000000);

    frame.distanceMm[0] = 2500;
    frame.distanceMm[7] = 2800;
    frame.distanceMm[56] = 3000;
    frame.distanceMm[63] = 3200;

    const auto plane =
        estimator.estimate(
            frame,
            {});

    TEST_ASSERT_TRUE(plane.valid);

    TEST_ASSERT_TRUE(
        plane.accepted < plane.candidates);

    TEST_ASSERT_FLOAT_WITHIN(
        8.0F,
        750.0F,
        plane.interceptMm);

    TEST_ASSERT_FLOAT_WITHIN(
        0.03F,
        0.12F,
        plane.slopeX);

    TEST_ASSERT_FLOAT_WITHIN(
        0.03F,
        0.08F,
        plane.slopeY);
}

void test_status_6_and_9_remain_usable_at_lower_confidence() {
    TofPlaneEstimator estimator;

    const auto status6 =
        estimator.estimate(
            makeFlatWall(
                700,
                1000000,
                {},
                6),
            {});

    const auto status9 =
        estimator.estimate(
            makeFlatWall(
                700,
                1000000,
                {},
                9),
            {});

    TEST_ASSERT_TRUE(status6.valid);
    TEST_ASSERT_TRUE(status9.valid);
    TEST_ASSERT_EQUAL_UINT8(64, status6.candidates);
    TEST_ASSERT_EQUAL_UINT8(64, status9.candidates);
}

void test_too_few_valid_zones_fail_closed() {
    TofPlaneEstimator estimator;

    auto frame =
        makeFlatWall(
            800,
            1000000);

    for (std::size_t index = 0; index < 64; ++index) {
        frame.targetStatus[index] =
            index < 10
                ? 5
                : 1;
    }

    const auto plane =
        estimator.estimate(
            frame,
            {});

    TEST_ASSERT_FALSE(plane.valid);
    TEST_ASSERT_EQUAL_UINT8(10, plane.candidates);
}

void test_transform_does_not_change_recovered_normalized_plane() {
    TofGridTransform transform;
    transform.rotation =
        ambilight::TofRotation::Deg270;
    transform.mirrorX = true;

    TofPlaneEstimator estimator;

    const auto frame =
        makePlane(
            850.0,
            0.18,
            -0.07,
            1000000,
            transform);

    const auto plane =
        estimator.estimate(
            frame,
            transform);

    TEST_ASSERT_TRUE(plane.valid);

    TEST_ASSERT_FLOAT_WITHIN(
        0.02F,
        0.18F,
        plane.slopeX);

    TEST_ASSERT_FLOAT_WITHIN(
        0.02F,
        -0.07F,
        plane.slopeY);
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_flat_wall_uses_perpendicular_distance_contract);
    RUN_TEST(test_combined_yaw_and_pitch_are_recovered);
    RUN_TEST(test_extreme_outliers_are_rejected);
    RUN_TEST(test_status_6_and_9_remain_usable_at_lower_confidence);
    RUN_TEST(test_too_few_valid_zones_fail_closed);
    RUN_TEST(test_transform_does_not_change_recovered_normalized_plane);

    return UNITY_END();
}
