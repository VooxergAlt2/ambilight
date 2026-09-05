#include <array>
#include <cstdint>

#include <unity.h>

#include "tof/TofGainModel.h"

using ambilight::DistanceGainCurve;
using ambilight::GainPoint;
using ambilight::TofGainModel;
using ambilight::TofGainModelConfig;
using ambilight::TofGeometrySnapshot;
using ambilight::kGainUnityQ12;

namespace {

DistanceGainCurve makeTestCurve() {
    std::array<GainPoint, DistanceGainCurve::kMaxPoints> points{};

    points[0] = {200, 2048}; // 0.50
    points[1] = {400, 3072}; // 0.75
    points[2] = {800, 4096}; // 1.00

    return DistanceGainCurve(points, 3);
}

TofGeometrySnapshot makeGeometry(
    std::uint16_t left,
    std::uint16_t center,
    std::uint16_t right,
    std::uint64_t timestampUs) {

    TofGeometrySnapshot geometry;
    geometry.valid = true;
    geometry.timestampUs = timestampUs;

    geometry.left.valid = true;
    geometry.left.filteredMm = left;

    geometry.center.valid = true;
    geometry.center.filteredMm = center;

    geometry.right.valid = true;
    geometry.right.filteredMm = right;

    return geometry;
}

} // namespace

void test_default_curve_is_pass_through() {
    DistanceGainCurve curve;

    TEST_ASSERT_TRUE(curve.valid());
    TEST_ASSERT_EQUAL_UINT16(
        kGainUnityQ12,
        curve.evaluate(50));
    TEST_ASSERT_EQUAL_UINT16(
        kGainUnityQ12,
        curve.evaluate(500));
    TEST_ASSERT_EQUAL_UINT16(
        kGainUnityQ12,
        curve.evaluate(4000));
}

void test_curve_interpolates_and_clamps() {
    const auto curve = makeTestCurve();

    TEST_ASSERT_TRUE(curve.valid());

    TEST_ASSERT_EQUAL_UINT16(2048, curve.evaluate(100));
    TEST_ASSERT_EQUAL_UINT16(2048, curve.evaluate(200));
    TEST_ASSERT_EQUAL_UINT16(2560, curve.evaluate(300));
    TEST_ASSERT_EQUAL_UINT16(3072, curve.evaluate(400));
    TEST_ASSERT_EQUAL_UINT16(3584, curve.evaluate(600));
    TEST_ASSERT_EQUAL_UINT16(4096, curve.evaluate(800));
    TEST_ASSERT_EQUAL_UINT16(4096, curve.evaluate(1200));
}

void test_invalid_curve_rejects_gain_above_unity() {
    std::array<GainPoint, DistanceGainCurve::kMaxPoints> points{};

    points[0] = {200, 2048};
    points[1] = {400, 5000};

    DistanceGainCurve curve(points, 2);

    TEST_ASSERT_FALSE(curve.valid());
    TEST_ASSERT_EQUAL_UINT16(
        kGainUnityQ12,
        curve.evaluate(300));
}

void test_invalid_curve_rejects_non_monotonic_distance() {
    std::array<GainPoint, DistanceGainCurve::kMaxPoints> points{};

    points[0] = {400, 2048};
    points[1] = {300, 3072};

    DistanceGainCurve curve(points, 2);

    TEST_ASSERT_FALSE(curve.valid());
}

void test_invalid_curve_rejects_decreasing_gain() {
    std::array<GainPoint, DistanceGainCurve::kMaxPoints> points{};

    points[0] = {200, 3072};
    points[1] = {400, 2048};

    DistanceGainCurve curve(points, 2);

    TEST_ASSERT_FALSE(curve.valid());
}

void test_valid_geometry_maps_to_four_side_gains() {
    TofGainModelConfig config;
    config.curve = makeTestCurve();

    TofGainModel model(config);

    const auto geometry =
        makeGeometry(200, 400, 800, 1000000);

    const auto gains =
        model.evaluate(geometry, 1100000);

    TEST_ASSERT_TRUE(gains.geometryUsable);
    TEST_ASSERT_FALSE(gains.failOpen);

    TEST_ASSERT_EQUAL_UINT16(2048, gains.leftQ12);
    TEST_ASSERT_EQUAL_UINT16(3072, gains.topQ12);
    TEST_ASSERT_EQUAL_UINT16(3072, gains.bottomQ12);
    TEST_ASSERT_EQUAL_UINT16(4096, gains.rightQ12);
}


void test_runtime_curve_replacement_changes_output() {
    TofGainModelConfig config;
    config.curve = makeTestCurve();

    TofGainModel model(config);

    const auto geometry =
        makeGeometry(
            300,
            300,
            300,
            1000000);

    const auto before =
        model.evaluate(
            geometry,
            1100000);

    TEST_ASSERT_EQUAL_UINT16(
        2560,
        before.leftQ12);

    std::array<
        GainPoint,
        DistanceGainCurve::kMaxPoints>
        points{};

    points[0] = {200, 1024};
    points[1] = {400, 2048};
    points[2] = {800, 4096};

    const DistanceGainCurve replacement(
        points,
        3);

    TEST_ASSERT_TRUE(
        model.setCurve(
            replacement));

    const auto after =
        model.evaluate(
            geometry,
            1200000);

    TEST_ASSERT_EQUAL_UINT16(
        1536,
        after.leftQ12);

    TEST_ASSERT_EQUAL_UINT16(
        1536,
        after.topQ12);

    TEST_ASSERT_FALSE(
        after.failOpen);
}

void test_invalid_runtime_curve_is_rejected_and_previous_curve_is_retained() {
    TofGainModelConfig config;
    config.curve = makeTestCurve();

    TofGainModel model(config);

    std::array<
        GainPoint,
        DistanceGainCurve::kMaxPoints>
        invalidPoints{};

    invalidPoints[0] = {400, 3072};
    invalidPoints[1] = {300, 4096};

    const DistanceGainCurve invalidCurve(
        invalidPoints,
        2);

    TEST_ASSERT_FALSE(
        invalidCurve.valid());

    TEST_ASSERT_FALSE(
        model.setCurve(
            invalidCurve));

    const auto gains =
        model.evaluate(
            makeGeometry(
                300,
                300,
                300,
                1000000),
            1100000);

    TEST_ASSERT_EQUAL_UINT16(
        2560,
        gains.leftQ12);
}

void test_invalid_geometry_fails_open() {
    TofGainModelConfig config;
    config.curve = makeTestCurve();

    TofGainModel model(config);

    auto geometry =
        makeGeometry(200, 400, 800, 1000000);

    geometry.left.valid = false;
    geometry.valid = false;

    const auto gains =
        model.evaluate(geometry, 1100000);

    TEST_ASSERT_TRUE(gains.failOpen);
    TEST_ASSERT_FALSE(gains.geometryUsable);

    TEST_ASSERT_EQUAL_UINT16(kGainUnityQ12, gains.leftQ12);
    TEST_ASSERT_EQUAL_UINT16(kGainUnityQ12, gains.rightQ12);
    TEST_ASSERT_EQUAL_UINT16(kGainUnityQ12, gains.topQ12);
    TEST_ASSERT_EQUAL_UINT16(kGainUnityQ12, gains.bottomQ12);
}

void test_stale_geometry_fails_open() {
    TofGainModelConfig config;
    config.curve = makeTestCurve();
    config.staleTimeoutUs = 1000000;

    TofGainModel model(config);

    const auto geometry =
        makeGeometry(200, 400, 800, 1000000);

    const auto gains =
        model.evaluate(geometry, 2000001);

    TEST_ASSERT_TRUE(gains.failOpen);
    TEST_ASSERT_FALSE(gains.geometryUsable);
    TEST_ASSERT_EQUAL_UINT16(kGainUnityQ12, gains.leftQ12);
}

void test_future_timestamp_fails_open() {
    TofGainModelConfig config;
    config.curve = makeTestCurve();

    TofGainModel model(config);

    const auto geometry =
        makeGeometry(200, 400, 800, 2000000);

    const auto gains =
        model.evaluate(geometry, 1000000);

    TEST_ASSERT_TRUE(gains.failOpen);
    TEST_ASSERT_FALSE(gains.geometryUsable);
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_default_curve_is_pass_through);
    RUN_TEST(test_curve_interpolates_and_clamps);
    RUN_TEST(test_invalid_curve_rejects_gain_above_unity);
    RUN_TEST(test_invalid_curve_rejects_non_monotonic_distance);
    RUN_TEST(test_invalid_curve_rejects_decreasing_gain);
    RUN_TEST(test_valid_geometry_maps_to_four_side_gains);
    RUN_TEST(test_runtime_curve_replacement_changes_output);
    RUN_TEST(test_invalid_runtime_curve_is_rejected_and_previous_curve_is_retained);
    RUN_TEST(test_invalid_geometry_fails_open);
    RUN_TEST(test_stale_geometry_fails_open);
    RUN_TEST(test_future_timestamp_fails_open);

    return UNITY_END();
}
