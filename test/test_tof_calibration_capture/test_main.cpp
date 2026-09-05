#include <unity.h>

#include "tof/TofCalibrationCapture.h"

using ambilight::CalibrationCaptureSummary;
using ambilight::TofCalibrationCapture;
using ambilight::TofGeometrySnapshot;

namespace {

TofGeometrySnapshot makeGeometry(
    std::uint32_t generation,
    std::uint16_t left,
    std::uint16_t center,
    std::uint16_t right,
    std::uint16_t mad = 10,
    std::uint8_t accepted = 20) {

    TofGeometrySnapshot geometry;

    geometry.generation = generation;
    geometry.valid = true;

    geometry.left.valid = true;
    geometry.left.robustMedianMm = left;
    geometry.left.madMm = mad;
    geometry.left.accepted = accepted;

    geometry.center.valid = true;
    geometry.center.robustMedianMm = center;
    geometry.center.madMm = mad;
    geometry.center.accepted =
        accepted > 16 ? 16 : accepted;

    geometry.right.valid = true;
    geometry.right.robustMedianMm = right;
    geometry.right.madMm = mad;
    geometry.right.accepted = accepted;

    geometry.rightMinusLeftMm =
        static_cast<std::int16_t>(
            static_cast<std::int32_t>(right) -
            static_cast<std::int32_t>(left));

    return geometry;
}

} // namespace

void test_stable_pose_summary() {
    TofCalibrationCapture capture;
    capture.start(1000, 5000000);

    for (std::uint32_t i = 0; i < 50; ++i) {
        TEST_ASSERT_TRUE(
            capture.ingest(
                makeGeometry(
                    i + 1,
                    400,
                    600,
                    800)));
    }

    const CalibrationCaptureSummary summary =
        capture.finish();

    TEST_ASSERT_EQUAL_UINT32(50, summary.totalFrames);
    TEST_ASSERT_EQUAL_UINT32(50, summary.validFrames);

    TEST_ASSERT_EQUAL_UINT16(400, summary.left.medianMm);
    TEST_ASSERT_EQUAL_UINT16(600, summary.center.medianMm);
    TEST_ASSERT_EQUAL_UINT16(800, summary.right.medianMm);

    TEST_ASSERT_EQUAL_INT16(
        400,
        summary.medianRightMinusLeftMm);
}

void test_duplicate_generation_is_not_double_counted() {
    TofCalibrationCapture capture;
    capture.start(1000);

    const auto geometry =
        makeGeometry(7, 400, 600, 800);

    TEST_ASSERT_TRUE(capture.ingest(geometry));
    TEST_ASSERT_FALSE(capture.ingest(geometry));

    const auto summary = capture.finish();

    TEST_ASSERT_EQUAL_UINT32(1, summary.totalFrames);
    TEST_ASSERT_EQUAL_UINT32(1, summary.validFrames);
    TEST_ASSERT_EQUAL_UINT32(1, summary.duplicateFrames);
}

void test_invalid_geometry_counts_total_but_not_valid() {
    TofCalibrationCapture capture;
    capture.start(1000);

    auto geometry =
        makeGeometry(1, 400, 600, 800);

    geometry.valid = false;
    geometry.left.valid = false;

    TEST_ASSERT_FALSE(capture.ingest(geometry));

    const auto summary = capture.finish();

    TEST_ASSERT_EQUAL_UINT32(1, summary.totalFrames);
    TEST_ASSERT_EQUAL_UINT32(0, summary.validFrames);
    TEST_ASSERT_FALSE(summary.left.valid);
}

void test_percentile_summary_tracks_pose_jitter() {
    TofCalibrationCapture capture;
    capture.start(1000);

    for (std::uint32_t i = 0; i < 10; ++i) {
        capture.ingest(
            makeGeometry(
                i + 1,
                static_cast<std::uint16_t>(400 + i * 10),
                600,
                800,
                static_cast<std::uint16_t>(5 + i),
                static_cast<std::uint8_t>(20 - i)));
    }

    const auto summary = capture.finish();

    TEST_ASSERT_TRUE(summary.left.valid);

    TEST_ASSERT_EQUAL_UINT16(410, summary.left.p10Mm);
    TEST_ASSERT_EQUAL_UINT16(450, summary.left.medianMm);
    TEST_ASSERT_EQUAL_UINT16(480, summary.left.p90Mm);

    TEST_ASSERT_EQUAL_UINT8(11, summary.left.minAccepted);
    TEST_ASSERT_EQUAL_UINT8(20, summary.left.maxAccepted);
}

void test_capture_expiry() {
    TofCalibrationCapture capture;
    capture.start(1000, 5000);

    TEST_ASSERT_FALSE(capture.expired(5999));
    TEST_ASSERT_TRUE(capture.expired(6000));

    capture.cancel();
    TEST_ASSERT_FALSE(capture.expired(7000));
}

void test_capacity_is_bounded() {
    TofCalibrationCapture capture;
    capture.start(1000);

    for (std::uint32_t i = 0;
         i < TofCalibrationCapture::kMaxSamples + 5;
         ++i) {

        capture.ingest(
            makeGeometry(
                i + 1,
                400,
                600,
                800));
    }

    TEST_ASSERT_EQUAL_UINT32(
        TofCalibrationCapture::kMaxSamples,
        capture.storedSamples());

    const auto summary = capture.finish();

    TEST_ASSERT_EQUAL_UINT32(
        5,
        summary.overflowFrames);
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_stable_pose_summary);
    RUN_TEST(test_duplicate_generation_is_not_double_counted);
    RUN_TEST(test_invalid_geometry_counts_total_but_not_valid);
    RUN_TEST(test_percentile_summary_tracks_pose_jitter);
    RUN_TEST(test_capture_expiry);
    RUN_TEST(test_capacity_is_bounded);

    return UNITY_END();
}
