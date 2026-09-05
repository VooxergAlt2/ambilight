#include <cmath>
#include <cstdint>

#include <unity.h>

#include "tof/TofGrid.h"
#include "tof/TofPlaneEstimator.h"

using ambilight::TofGridTransform;
using ambilight::TofPlaneEstimator;
using ambilight::TofPlaneEstimatorConfig;
using ambilight::TofRawFrame;

namespace {

constexpr double kPi = 3.14159265358979323846;

TofRawFrame makePlane(
    double interceptMm,
    double slopeX,
    double slopeY,
    std::uint64_t timestampUs,
    const TofGridTransform& transform = {},
    std::uint8_t status = 5) {

    TofRawFrame frame;
    frame.timestampUs = timestampUs;

    const double diagonalHalf =
        65.0 * kPi / 180.0 / 2.0;

    const double tanHalfAxis =
        std::tan(diagonalHalf) /
        std::sqrt(2.0);

    for (std::size_t row = 0; row < 8; ++row) {
        for (std::size_t col = 0; col < 8; ++col) {
            const double nx =
                (static_cast<double>(col) - 3.5) / 4.0;

            const double ny =
                (3.5 - static_cast<double>(row)) / 4.0;

            const double rx = nx * tanHalfAxis;
            const double ry = ny * tanHalfAxis;
            constexpr double rz = 1.0;

            const double norm =
                std::sqrt(
                    rx * rx +
                    ry * ry +
                    rz * rz);

            const double dx = rx / norm;
            const double dy = ry / norm;
            const double dz = rz / norm;

            const double denominator =
                dz -
                slopeX * dx -
                slopeY * dy;

            const double distance =
                interceptMm / denominator;

            const std::size_t rawIndex =
                ambilight::tofRawIndexForNormalized(
                    row,
                    col,
                    transform);

            frame.distanceMm[rawIndex] =
                static_cast<std::int16_t>(
                    std::lround(distance));

            frame.targetStatus[rawIndex] =
                status;
        }
    }

    return frame;
}

} // namespace

void test_flat_wall_recovers_zero_slopes() {
    TofPlaneEstimator estimator;

    const auto plane =
        estimator.estimate(
            makePlane(
                800.0,
                0.0,
                0.0,
                1000000),
            {});

    TEST_ASSERT_TRUE(plane.valid);
    TEST_ASSERT_EQUAL_UINT8(64, plane.candidates);
    TEST_ASSERT_TRUE(plane.accepted >= 60);

    TEST_ASSERT_FLOAT_WITHIN(
        2.0F,
        800.0F,
        plane.interceptMm);

    TEST_ASSERT_FLOAT_WITHIN(
        0.005F,
        0.0F,
        plane.slopeX);

    TEST_ASSERT_FLOAT_WITHIN(
        0.005F,
        0.0F,
        plane.slopeY);

    TEST_ASSERT_INT_WITHIN(
        30,
        0,
        plane.yawCentiDeg);

    TEST_ASSERT_INT_WITHIN(
        30,
        0,
        plane.pitchCentiDeg);

    TEST_ASSERT_TRUE(
        plane.observedHalfSpanXmm > 200);

    TEST_ASSERT_TRUE(
        plane.observedHalfSpanYmm > 200);
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

void test_status_9_remains_usable() {
    TofPlaneEstimator estimator;

    const auto plane =
        estimator.estimate(
            makePlane(
                700.0,
                -0.10,
                0.05,
                1000000,
                {},
                9),
            {});

    TEST_ASSERT_TRUE(plane.valid);
    TEST_ASSERT_EQUAL_UINT8(64, plane.candidates);

    TEST_ASSERT_FLOAT_WITHIN(
        0.02F,
        -0.10F,
        plane.slopeX);
}

void test_too_few_valid_zones_fail_closed() {
    TofPlaneEstimator estimator;

    auto frame =
        makePlane(
            800.0,
            0.0,
            0.0,
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

    RUN_TEST(test_flat_wall_recovers_zero_slopes);
    RUN_TEST(test_combined_yaw_and_pitch_are_recovered);
    RUN_TEST(test_extreme_outliers_are_rejected);
    RUN_TEST(test_status_9_remains_usable);
    RUN_TEST(test_too_few_valid_zones_fail_closed);
    RUN_TEST(test_transform_does_not_change_recovered_normalized_plane);

    return UNITY_END();
}
