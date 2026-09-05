#include <array>
#include <cstdint>

#include <unity.h>

#include "tof/TofPerimeterGainModel.h"

using ambilight::DistanceGainCurve;
using ambilight::GainPoint;
using ambilight::PerimeterScreenGeometry;
using ambilight::ScreenPointMm;
using ambilight::SegmentId;
using ambilight::SegmentScreenGeometry;
using ambilight::TofGeometrySnapshot;
using ambilight::TofPerimeterGainModel;
using ambilight::TofPerimeterGainModelConfig;
using ambilight::kGainUnityQ12;

namespace {

PerimeterScreenGeometry makeRectangle(
    float widthMm,
    float heightMm) {

    const float hw = widthMm / 2.0F;
    const float hh = heightMm / 2.0F;

    return {{
        {
            SegmentId::Top,
            {-hw, +hh, 0.0F},
            {+hw, +hh, 0.0F}
        },
        {
            SegmentId::Right,
            {+hw, +hh, 0.0F},
            {+hw, -hh, 0.0F}
        },
        {
            SegmentId::Bottom,
            {+hw, -hh, 0.0F},
            {-hw, -hh, 0.0F}
        },
        {
            SegmentId::Left,
            {-hw, -hh, 0.0F},
            {-hw, +hh, 0.0F}
        }
    }};
}

DistanceGainCurve makeCurve() {
    std::array<
        GainPoint,
        DistanceGainCurve::kMaxPoints>
        points{};

    points[0] = {100, 1024};
    points[1] = {500, 2048};
    points[2] = {900, 4096};

    return DistanceGainCurve(
        points,
        3);
}

TofGeometrySnapshot makePlane(
    float interceptMm,
    float slopeX,
    float slopeY,
    std::uint64_t timestampUs) {

    TofGeometrySnapshot geometry;
    geometry.timestampUs = timestampUs;

    geometry.plane.valid = true;
    geometry.plane.interceptMm = interceptMm;
    geometry.plane.slopeX = slopeX;
    geometry.plane.slopeY = slopeY;
    geometry.plane.observedHalfSpanXmm = 500;
    geometry.plane.observedHalfSpanYmm = 250;

    return geometry;
}

TofPerimeterGainModel makeModel() {
    TofPerimeterGainModelConfig config;
    config.curve = makeCurve();
    config.geometry =
        makeRectangle(
            1000.0F,
            500.0F);

    return TofPerimeterGainModel(config);
}

const ambilight::PerimeterSegmentGain& segment(
    const ambilight::PerimeterGainSnapshot& snapshot,
    SegmentId id) {

    return snapshot.segment[
        static_cast<std::size_t>(id)];
}

} // namespace

void test_flat_wall_produces_uniform_perimeter() {
    auto model = makeModel();

    const auto result =
        model.evaluate(
            makePlane(
                600.0F,
                0.0F,
                0.0F,
                1000000),
            1100000);

    TEST_ASSERT_FALSE(result.failOpen);
    TEST_ASSERT_TRUE(result.planeUsable);

    for (std::size_t index = 0; index < 4; ++index) {
        TEST_ASSERT_EQUAL_UINT16(
            600,
            result.segment[index].startDistanceMm);

        TEST_ASSERT_EQUAL_UINT16(
            600,
            result.segment[index].endDistanceMm);

        TEST_ASSERT_EQUAL_UINT16(
            result.segment[index].startQ12,
            result.segment[index].endQ12);
    }

    // 600 mm lies halfway between the 500 mm / 2048 and
    // 900 mm / 4096 calibration points.
    for (const auto gain :
         result.logicalGainQ12) {

        TEST_ASSERT_EQUAL_UINT16(
            2560,
            gain);
    }
}

void test_yaw_creates_horizontal_within_segment_gradients() {
    auto model = makeModel();

    const auto result =
        model.evaluate(
            makePlane(
                600.0F,
                0.20F,
                0.0F,
                1000000),
            1100000);

    const auto& top =
        segment(result, SegmentId::Top);

    const auto& bottom =
        segment(result, SegmentId::Bottom);

    const auto& right =
        segment(result, SegmentId::Right);

    const auto& left =
        segment(result, SegmentId::Left);

    TEST_ASSERT_EQUAL_UINT16(
        500,
        top.startDistanceMm);

    TEST_ASSERT_EQUAL_UINT16(
        700,
        top.endDistanceMm);

    TEST_ASSERT_EQUAL_UINT16(
        700,
        bottom.startDistanceMm);

    TEST_ASSERT_EQUAL_UINT16(
        500,
        bottom.endDistanceMm);

    TEST_ASSERT_EQUAL_UINT16(
        700,
        right.startDistanceMm);

    TEST_ASSERT_EQUAL_UINT16(
        700,
        right.endDistanceMm);

    TEST_ASSERT_EQUAL_UINT16(
        500,
        left.startDistanceMm);

    TEST_ASSERT_EQUAL_UINT16(
        500,
        left.endDistanceMm);

    TEST_ASSERT_TRUE(
        top.startQ12 < top.endQ12);

    TEST_ASSERT_TRUE(
        bottom.startQ12 > bottom.endQ12);
}

void test_pitch_creates_vertical_within_segment_gradients() {
    auto model = makeModel();

    const auto result =
        model.evaluate(
            makePlane(
                600.0F,
                0.0F,
                0.20F,
                1000000),
            1100000);

    const auto& top =
        segment(result, SegmentId::Top);

    const auto& bottom =
        segment(result, SegmentId::Bottom);

    const auto& right =
        segment(result, SegmentId::Right);

    const auto& left =
        segment(result, SegmentId::Left);

    // half-height = 250 mm; slope 0.20 => +/-50 mm.
    TEST_ASSERT_EQUAL_UINT16(
        650,
        top.startDistanceMm);

    TEST_ASSERT_EQUAL_UINT16(
        650,
        top.endDistanceMm);

    TEST_ASSERT_EQUAL_UINT16(
        550,
        bottom.startDistanceMm);

    TEST_ASSERT_EQUAL_UINT16(
        550,
        bottom.endDistanceMm);

    // RIGHT logical direction is top -> bottom.
    TEST_ASSERT_EQUAL_UINT16(
        650,
        right.startDistanceMm);

    TEST_ASSERT_EQUAL_UINT16(
        550,
        right.endDistanceMm);

    // LEFT logical direction is bottom -> top.
    TEST_ASSERT_EQUAL_UINT16(
        550,
        left.startDistanceMm);

    TEST_ASSERT_EQUAL_UINT16(
        650,
        left.endDistanceMm);

    TEST_ASSERT_TRUE(
        right.startQ12 > right.endQ12);

    TEST_ASSERT_TRUE(
        left.startQ12 < left.endQ12);
}

void test_combined_yaw_pitch_changes_all_four_segments() {
    auto model = makeModel();

    const auto result =
        model.evaluate(
            makePlane(
                700.0F,
                0.20F,
                -0.20F,
                1000000),
            1100000);

    TEST_ASSERT_FALSE(result.failOpen);

    for (std::size_t index = 0; index < 4; ++index) {
        TEST_ASSERT_NOT_EQUAL(
            result.segment[index].startDistanceMm,
            result.segment[index].endDistanceMm);
    }

    TEST_ASSERT_TRUE(
        result.maxDistanceMm >
        result.minDistanceMm);
}

void test_per_pixel_curve_evaluation_is_exact_across_calibration_knot() {
    auto model = makeModel();

    // Width 1000 mm, slopeX=0.40 around intercept 500 mm:
    // TOP distance runs 300 -> 700 mm and crosses the 500 mm curve knot.
    const auto result =
        model.evaluate(
            makePlane(
                500.0F,
                0.40F,
                0.0F,
                1000000),
            1100000);

    TEST_ASSERT_FALSE(result.failOpen);

    const auto& top =
        segment(result, SegmentId::Top);

    TEST_ASSERT_EQUAL_UINT16(
        300,
        top.startDistanceMm);

    TEST_ASSERT_EQUAL_UINT16(
        700,
        top.endDistanceMm);

    // TOP has 230 LEDs, denominator 229.
    // At logical offset 114:
    // d = round(300 + 400*114/229) = 499 mm.
    // The real calibration curve gives Q12=2045.
    //
    // Linear interpolation between endpoint gains would be about 2300 and is
    // intentionally NOT what Stage 17 does.
    TEST_ASSERT_EQUAL_UINT16(
        2045,
        result.logicalGainQ12[114]);

    TEST_ASSERT_TRUE(
        result.logicalGainQ12[114] <
        2100);

    // Next LED is just across the 500 mm knot.
    TEST_ASSERT_TRUE(
        result.logicalGainQ12[115] >
        result.logicalGainQ12[114]);
}

void test_led_plane_z_offset_is_subtracted() {
    TofPerimeterGainModelConfig config;
    config.curve = makeCurve();

    auto geometry =
        makeRectangle(
            1000.0F,
            500.0F);

    for (auto& segmentGeometry : geometry) {
        segmentGeometry.logicalStart.zMm = 20.0F;
        segmentGeometry.logicalEnd.zMm = 20.0F;
    }

    config.geometry = geometry;

    TofPerimeterGainModel model(config);

    const auto result =
        model.evaluate(
            makePlane(
                600.0F,
                0.0F,
                0.0F,
                1000000),
            1100000);

    TEST_ASSERT_EQUAL_UINT16(
        580,
        segment(
            result,
            SegmentId::Top).startDistanceMm);
}

void test_invalid_or_stale_plane_fails_open() {
    auto model = makeModel();

    auto geometry =
        makePlane(
            600.0F,
            0.0F,
            0.0F,
            1000000);

    geometry.plane.valid = false;

    auto invalid =
        model.evaluate(
            geometry,
            1100000);

    TEST_ASSERT_TRUE(invalid.failOpen);
    TEST_ASSERT_FALSE(invalid.planeUsable);

    geometry.plane.valid = true;

    auto stale =
        model.evaluate(
            geometry,
            3000000);

    TEST_ASSERT_TRUE(stale.failOpen);

    for (const auto& s : stale.segment) {
        TEST_ASSERT_EQUAL_UINT16(
            kGainUnityQ12,
            s.startQ12);

        TEST_ASSERT_EQUAL_UINT16(
            kGainUnityQ12,
            s.endQ12);
    }
}

void test_large_extrapolation_warns_but_does_not_fail_open() {
    TofPerimeterGainModelConfig config;
    config.curve = makeCurve();
    config.geometry =
        makeRectangle(
            1000.0F,
            500.0F);
    config.maxRecommendedExtrapolationPermille = 4000;

    TofPerimeterGainModel model(config);

    auto geometry =
        makePlane(
            600.0F,
            0.05F,
            0.03F,
            1000000);

    geometry.plane.observedHalfSpanXmm = 100;
    geometry.plane.observedHalfSpanYmm = 100;

    const auto result =
        model.evaluate(
            geometry,
            1100000);

    TEST_ASSERT_FALSE(result.failOpen);
    TEST_ASSERT_TRUE(result.planeUsable);
    TEST_ASSERT_TRUE(result.projectionUsable);
    TEST_ASSERT_TRUE(result.extrapolationWarning);

    TEST_ASSERT_EQUAL_UINT16(
        5000,
        result.extrapolationXPermille);

    TEST_ASSERT_EQUAL_UINT16(
        2500,
        result.extrapolationYPermille);
}

void test_predicted_endpoint_outside_range_fails_open() {
    auto model = makeModel();

    const auto result =
        model.evaluate(
            makePlane(
                100.0F,
                -0.30F,
                0.0F,
                1000000),
            1100000);

    TEST_ASSERT_TRUE(result.failOpen);
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_flat_wall_produces_uniform_perimeter);
    RUN_TEST(test_yaw_creates_horizontal_within_segment_gradients);
    RUN_TEST(test_pitch_creates_vertical_within_segment_gradients);
    RUN_TEST(test_combined_yaw_pitch_changes_all_four_segments);
    RUN_TEST(test_per_pixel_curve_evaluation_is_exact_across_calibration_knot);
    RUN_TEST(test_led_plane_z_offset_is_subtracted);
    RUN_TEST(test_invalid_or_stale_plane_fails_open);
    RUN_TEST(test_large_extrapolation_warns_but_does_not_fail_open);
    RUN_TEST(test_predicted_endpoint_outside_range_fails_open);

    return UNITY_END();
}
