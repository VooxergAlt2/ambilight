#include <cstdint>

#include <unity.h>

#include "tof/TofProcessor.h"
#include "tof/TofDebugGrid.h"

using ambilight::TofGridTransform;
using ambilight::TofProcessor;
using ambilight::TofProcessorConfig;
using ambilight::TofRawFrame;
using ambilight::TofRotation;

namespace {

void setNormalized(
    TofRawFrame& frame,
    std::size_t row,
    std::size_t col,
    std::int16_t distance,
    std::uint8_t status,
    const TofGridTransform& transform = {}) {

    const std::size_t rawIndex =
        TofProcessor::rawIndexForNormalized(
            row,
            col,
            transform);

    frame.distanceMm[rawIndex] = distance;
    frame.targetStatus[rawIndex] = status;
}

TofRawFrame makeFlat(
    std::uint16_t distanceMm,
    std::uint64_t timestampUs,
    const TofGridTransform& transform = {}) {

    TofRawFrame frame;
    frame.timestampUs = timestampUs;

    for (std::size_t row = 0; row < 8; ++row) {
        for (std::size_t col = 0; col < 8; ++col) {
            setNormalized(
                frame,
                row,
                col,
                static_cast<std::int16_t>(distanceMm),
                5,
                transform);
        }
    }

    return frame;
}

} // namespace

void test_raw_index_transform_has_expected_indices() {
    TofGridTransform transform;

    TEST_ASSERT_EQUAL_UINT32(
        10,
        TofProcessor::rawIndexForNormalized(
            1, 2, transform));

    transform.rotation = TofRotation::Deg90;
    TEST_ASSERT_EQUAL_UINT32(
        41,
        TofProcessor::rawIndexForNormalized(
            1, 2, transform));

    transform.rotation = TofRotation::Deg180;
    TEST_ASSERT_EQUAL_UINT32(
        53,
        TofProcessor::rawIndexForNormalized(
            1, 2, transform));

    transform.rotation = TofRotation::Deg270;
    TEST_ASSERT_EQUAL_UINT32(
        22,
        TofProcessor::rawIndexForNormalized(
            1, 2, transform));

    transform.rotation = TofRotation::Deg0;
    transform.mirrorX = true;
    TEST_ASSERT_EQUAL_UINT32(
        13,
        TofProcessor::rawIndexForNormalized(
            1, 2, transform));
}

void test_flat_wall_produces_equal_bands() {
    TofProcessor processor;
    const auto result =
        processor.process(makeFlat(600, 1000000));

    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_UINT16(600, result.left.robustMedianMm);
    TEST_ASSERT_EQUAL_UINT16(600, result.center.robustMedianMm);
    TEST_ASSERT_EQUAL_UINT16(600, result.right.robustMedianMm);
    TEST_ASSERT_EQUAL_UINT16(600, result.left.filteredMm);
    TEST_ASSERT_EQUAL_UINT16(600, result.center.filteredMm);
    TEST_ASSERT_EQUAL_UINT16(600, result.right.filteredMm);
    TEST_ASSERT_EQUAL_INT16(0, result.rightMinusLeftMm);
    TEST_ASSERT_EQUAL_UINT8(64, result.acceptedZones);
}

void test_normalized_left_center_right_are_independent() {
    TofProcessor processor;
    TofRawFrame frame;
    frame.timestampUs = 1000000;

    for (std::size_t row = 0; row < 8; ++row) {
        for (std::size_t col = 0; col < 8; ++col) {
            const std::uint16_t distance =
                col <= 2 ? 400 : (col <= 4 ? 600 : 900);

            setNormalized(
                frame,
                row,
                col,
                static_cast<std::int16_t>(distance),
                5);
        }
    }

    const auto result = processor.process(frame);

    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_UINT16(400, result.left.filteredMm);
    TEST_ASSERT_EQUAL_UINT16(600, result.center.filteredMm);
    TEST_ASSERT_EQUAL_UINT16(900, result.right.filteredMm);
    TEST_ASSERT_EQUAL_INT16(500, result.rightMinusLeftMm);
}

void test_single_far_outlier_is_rejected() {
    TofProcessor processor;
    auto frame = makeFlat(700, 1000000);

    setNormalized(frame, 0, 0, 3500, 5);

    const auto result = processor.process(frame);

    TEST_ASSERT_TRUE(result.left.valid);
    TEST_ASSERT_EQUAL_UINT16(700, result.left.rawMedianMm);
    TEST_ASSERT_EQUAL_UINT16(700, result.left.robustMedianMm);
    TEST_ASSERT_EQUAL_UINT8(23, result.left.accepted);
}

void test_invalid_statuses_and_ranges_are_ignored() {
    TofProcessor processor;
    auto frame = makeFlat(650, 1000000);

    setNormalized(frame, 0, 0, 650, 1);
    setNormalized(frame, 0, 1, 650, 4);
    setNormalized(frame, 0, 2, 0, 5);
    setNormalized(frame, 1, 0, 4500, 5);
    setNormalized(frame, 1, 1, 40, 9);

    const auto result = processor.process(frame);

    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_UINT8(19, result.left.candidates);
    TEST_ASSERT_EQUAL_UINT16(650, result.left.robustMedianMm);
}

void test_insufficient_valid_side_marks_geometry_invalid() {
    TofProcessor processor;
    auto frame = makeFlat(600, 1000000);

    for (std::size_t row = 0; row < 8; ++row) {
        for (std::size_t col = 0; col <= 2; ++col) {
            setNormalized(frame, row, col, 600, 1);
        }
    }

    for (std::size_t i = 0; i < 5; ++i) {
        const std::size_t row = i / 3;
        const std::size_t col = i % 3;
        setNormalized(frame, row, col, 600, 5);
    }

    const auto result = processor.process(frame);

    TEST_ASSERT_FALSE(result.left.valid);
    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_EQUAL_UINT8(5, result.left.candidates);
}

void test_rotation_and_mirror_normalize_physical_grid() {
    TofProcessorConfig config;
    config.transform.rotation = TofRotation::Deg90;
    config.transform.mirrorX = true;

    TofProcessor processor(config);
    TofRawFrame frame;
    frame.timestampUs = 1000000;

    for (std::size_t row = 0; row < 8; ++row) {
        for (std::size_t col = 0; col < 8; ++col) {
            const std::uint16_t distance =
                col <= 2 ? 300 : (col <= 4 ? 500 : 800);

            setNormalized(
                frame,
                row,
                col,
                static_cast<std::int16_t>(distance),
                5,
                config.transform);
        }
    }

    const auto result = processor.process(frame);

    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_UINT16(300, result.left.filteredMm);
    TEST_ASSERT_EQUAL_UINT16(500, result.center.filteredMm);
    TEST_ASSERT_EQUAL_UINT16(800, result.right.filteredMm);
}


void test_runtime_transform_change_resets_temporal_state() {
    TofProcessorConfig config;
    config.filterTimeConstantMs = 600;
    config.deadbandMm = 0;

    TofProcessor processor(config);

    processor.process(
        makeFlat(
            400,
            1000000));

    TofGridTransform transform;
    transform.rotation =
        TofRotation::Deg90;
    transform.mirrorX = true;

    processor.setTransform(
        transform);

    const auto result =
        processor.process(
            makeFlat(
                900,
                1100000,
                transform));

    // setTransform() must reset the old 400 mm temporal state. The first
    // sample in the new coordinate system initializes immediately at 900.
    TEST_ASSERT_EQUAL_UINT16(
        900,
        result.left.filteredMm);

    TEST_ASSERT_EQUAL_UINT16(
        900,
        result.center.filteredMm);

    TEST_ASSERT_EQUAL_UINT16(
        900,
        result.right.filteredMm);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            TofRotation::Deg90),
        static_cast<int>(
            processor.transform().rotation));

    TEST_ASSERT_TRUE(
        processor.transform().mirrorX);
}

void test_temporal_filter_smooths_step_change_both_directions() {
    TofProcessorConfig config;
    config.filterTimeConstantMs = 600;
    config.deadbandMm = 0;

    TofProcessor processor(config);

    const auto r1 =
        processor.process(makeFlat(500, 1000000));

    TEST_ASSERT_EQUAL_UINT16(500, r1.left.filteredMm);

    const auto r2 =
        processor.process(makeFlat(1100, 1100000));

    TEST_ASSERT_TRUE(r2.left.filteredMm > 570);
    TEST_ASSERT_TRUE(r2.left.filteredMm < 600);

    const auto r3 =
        processor.process(makeFlat(300, 1200000));

    TEST_ASSERT_TRUE(
        r3.left.filteredMm < r2.left.filteredMm);
    TEST_ASSERT_TRUE(r3.left.filteredMm > 300);
}

void test_deadband_holds_small_jitter() {
    TofProcessorConfig config;
    config.deadbandMm = 10;

    TofProcessor processor(config);

    processor.process(makeFlat(700, 1000000));
    const auto result =
        processor.process(makeFlat(707, 1100000));

    TEST_ASSERT_EQUAL_UINT16(700, result.left.filteredMm);
    TEST_ASSERT_EQUAL_UINT16(700, result.center.filteredMm);
    TEST_ASSERT_EQUAL_UINT16(700, result.right.filteredMm);
}

void test_status_9_is_accepted() {
    TofProcessor processor;
    TofRawFrame frame;
    frame.timestampUs = 1000000;

    for (std::size_t row = 0; row < 8; ++row) {
        for (std::size_t col = 0; col < 8; ++col) {
            setNormalized(frame, row, col, 750, 9);
        }
    }

    const auto result = processor.process(frame);

    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_UINT8(64, result.acceptedZones);
    TEST_ASSERT_EQUAL_UINT16(750, result.center.filteredMm);
}

void test_debug_grid_reports_normalized_zone_and_raw_index() {
    ambilight::TofRawFrame raw;

    for (std::size_t index = 0;
         index <
            ambilight::kTofZoneCount;
         ++index) {

        raw.distanceMm[index] =
            static_cast<std::int16_t>(
                1000 + index);

        raw.targetStatus[index] =
            index == 0
                ? 5
                : 12;
    }

    ambilight::TofGridTransform transform;
    transform.rotation =
        ambilight::TofRotation::Deg180;

    const auto grid =
        ambilight::makeTofDebugGrid(
            raw,
            transform);

    TEST_ASSERT_EQUAL_UINT8(
        63,
        grid[0].rawIndex);

    TEST_ASSERT_EQUAL_INT16(
        1063,
        grid[0].distanceMm);

    TEST_ASSERT_EQUAL_UINT8(
        0,
        grid[63].rawIndex);

    TEST_ASSERT_TRUE(
        grid[63].fullConfidence());

    TEST_ASSERT_TRUE(
        grid[63].usable());
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_debug_grid_reports_normalized_zone_and_raw_index);

    RUN_TEST(test_raw_index_transform_has_expected_indices);
    RUN_TEST(test_flat_wall_produces_equal_bands);
    RUN_TEST(test_normalized_left_center_right_are_independent);
    RUN_TEST(test_single_far_outlier_is_rejected);
    RUN_TEST(test_invalid_statuses_and_ranges_are_ignored);
    RUN_TEST(test_insufficient_valid_side_marks_geometry_invalid);
    RUN_TEST(test_rotation_and_mirror_normalize_physical_grid);
    RUN_TEST(test_runtime_transform_change_resets_temporal_state);
    RUN_TEST(test_temporal_filter_smooths_step_change_both_directions);
    RUN_TEST(test_deadband_holds_small_jitter);
    RUN_TEST(test_status_9_is_accepted);

    return UNITY_END();
}
