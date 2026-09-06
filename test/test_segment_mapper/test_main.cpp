#include <unity.h>

#include "core/Geometry.h"
#include "led/SegmentMapper.h"
#include "led/LedPixelMaskProfile.h"

using ambilight::LedMappingProfile;
using ambilight::LedPixelMaskProfile;
using ambilight::PhysicalPixel;
using ambilight::SegmentConfig;
using ambilight::SegmentId;
using ambilight::SegmentMapper;

void test_default_geometry_boundaries() {
    const PhysicalPixel p0 = SegmentMapper::map(0);
    TEST_ASSERT_TRUE(p0.valid);
    TEST_ASSERT_EQUAL_UINT8(0, p0.lane);
    TEST_ASSERT_EQUAL_UINT16(0, p0.index);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(SegmentId::Top),
        static_cast<std::uint8_t>(p0.segment));
    TEST_ASSERT_EQUAL_UINT16(0, p0.segmentOffset);
    TEST_ASSERT_EQUAL_UINT16(230, p0.segmentLength);

    const PhysicalPixel p229 = SegmentMapper::map(229);
    TEST_ASSERT_TRUE(p229.valid);
    TEST_ASSERT_EQUAL_UINT8(0, p229.lane);
    TEST_ASSERT_EQUAL_UINT16(229, p229.index);

    const PhysicalPixel p230 = SegmentMapper::map(230);
    TEST_ASSERT_TRUE(p230.valid);
    TEST_ASSERT_EQUAL_UINT8(1, p230.lane);
    TEST_ASSERT_EQUAL_UINT16(0, p230.index);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(SegmentId::Right),
        static_cast<std::uint8_t>(p230.segment));
    TEST_ASSERT_EQUAL_UINT16(0, p230.segmentOffset);
    TEST_ASSERT_EQUAL_UINT16(160, p230.segmentLength);

    const PhysicalPixel p389 = SegmentMapper::map(389);
    TEST_ASSERT_TRUE(p389.valid);
    TEST_ASSERT_EQUAL_UINT8(1, p389.lane);
    TEST_ASSERT_EQUAL_UINT16(159, p389.index);

    const PhysicalPixel p390 = SegmentMapper::map(390);
    TEST_ASSERT_TRUE(p390.valid);
    TEST_ASSERT_EQUAL_UINT8(2, p390.lane);
    TEST_ASSERT_EQUAL_UINT16(0, p390.index);

    const PhysicalPixel p619 = SegmentMapper::map(619);
    TEST_ASSERT_TRUE(p619.valid);
    TEST_ASSERT_EQUAL_UINT8(2, p619.lane);
    TEST_ASSERT_EQUAL_UINT16(229, p619.index);

    const PhysicalPixel p620 = SegmentMapper::map(620);
    TEST_ASSERT_TRUE(p620.valid);
    TEST_ASSERT_EQUAL_UINT8(3, p620.lane);
    TEST_ASSERT_EQUAL_UINT16(0, p620.index);

    const PhysicalPixel p779 = SegmentMapper::map(779);
    TEST_ASSERT_TRUE(p779.valid);
    TEST_ASSERT_EQUAL_UINT8(3, p779.lane);
    TEST_ASSERT_EQUAL_UINT16(159, p779.index);
}

void test_out_of_range_is_invalid() {
    const PhysicalPixel invalid = SegmentMapper::map(780);
    TEST_ASSERT_FALSE(invalid.valid);
}

void test_reversed_segment_mapping() {
    constexpr SegmentConfig reversed {
        SegmentId::Left,
        100,
        10,
        2,
        true
    };

    const PhysicalPixel first = SegmentMapper::mapInSegment(reversed, 100);
    TEST_ASSERT_TRUE(first.valid);
    TEST_ASSERT_EQUAL_UINT8(2, first.lane);
    TEST_ASSERT_EQUAL_UINT16(9, first.index);
    TEST_ASSERT_EQUAL_UINT16(0, first.segmentOffset);
    TEST_ASSERT_EQUAL_UINT16(10, first.segmentLength);

    const PhysicalPixel last = SegmentMapper::mapInSegment(reversed, 109);
    TEST_ASSERT_TRUE(last.valid);
    TEST_ASSERT_EQUAL_UINT8(2, last.lane);
    TEST_ASSERT_EQUAL_UINT16(0, last.index);
    TEST_ASSERT_EQUAL_UINT16(9, last.segmentOffset);

    const PhysicalPixel before = SegmentMapper::mapInSegment(reversed, 99);
    TEST_ASSERT_FALSE(before.valid);

    const PhysicalPixel after = SegmentMapper::mapInSegment(reversed, 110);
    TEST_ASSERT_FALSE(after.valid);
}


void test_runtime_profile_can_permute_lanes() {
    LedMappingProfile profile;

    profile.segment[0].lane = 3; // TOP -> lane 3
    profile.segment[1].lane = 2; // RIGHT -> lane 2
    profile.segment[2].lane = 1; // BOTTOM -> lane 1
    profile.segment[3].lane = 0; // LEFT -> lane 0

    TEST_ASSERT_TRUE(profile.valid());

    const auto top =
        SegmentMapper::map(
            0,
            profile);

    const auto right =
        SegmentMapper::map(
            230,
            profile);

    const auto bottom =
        SegmentMapper::map(
            390,
            profile);

    const auto left =
        SegmentMapper::map(
            620,
            profile);

    TEST_ASSERT_EQUAL_UINT8(3, top.lane);
    TEST_ASSERT_EQUAL_UINT8(2, right.lane);
    TEST_ASSERT_EQUAL_UINT8(1, bottom.lane);
    TEST_ASSERT_EQUAL_UINT8(0, left.lane);
}

void test_runtime_profile_reverses_only_selected_segment() {
    LedMappingProfile profile;
    profile.segment[
        static_cast<std::size_t>(
            SegmentId::Top)].reversed = 1;

    TEST_ASSERT_TRUE(profile.valid());

    const auto firstTop =
        SegmentMapper::map(
            0,
            profile);

    const auto lastTop =
        SegmentMapper::map(
            229,
            profile);

    const auto firstRight =
        SegmentMapper::map(
            230,
            profile);

    TEST_ASSERT_EQUAL_UINT16(
        229,
        firstTop.index);

    TEST_ASSERT_EQUAL_UINT16(
        0,
        lastTop.index);

    TEST_ASSERT_EQUAL_UINT16(
        0,
        firstRight.index);
}

void test_disabled_pixel_follows_logical_segment_offset_under_reversal() {
    LedMappingProfile mapping;
    mapping.segment[
        static_cast<std::size_t>(
            SegmentId::Top)].reversed = 1;

    LedPixelMaskProfile mask;
    mask.disabledOffset[
        static_cast<std::size_t>(
            SegmentId::Top)] = 7;

    const auto masked =
        SegmentMapper::map(
            7,
            mapping);

    TEST_ASSERT_TRUE(masked.valid);
    TEST_ASSERT_EQUAL_UINT16(
        7,
        masked.segmentOffset);

    TEST_ASSERT_EQUAL_UINT16(
        222,
        masked.index);

    TEST_ASSERT_TRUE(
        mask.disabled(
            masked.segment,
            masked.segmentOffset));

    const auto physicalSeven =
        SegmentMapper::map(
            222,
            mapping);

    TEST_ASSERT_TRUE(
        physicalSeven.valid);

    TEST_ASSERT_EQUAL_UINT16(
        7,
        physicalSeven.index);

    TEST_ASSERT_FALSE(
        mask.disabled(
            physicalSeven.segment,
            physicalSeven.segmentOffset));
}

void test_duplicate_runtime_lane_is_rejected() {
    LedMappingProfile profile;

    profile.segment[1].lane = 0;

    TEST_ASSERT_FALSE(
        profile.valid());

    const auto mapped =
        SegmentMapper::map(
            230,
            profile);

    TEST_ASSERT_FALSE(
        mapped.valid);
}

void test_invalid_runtime_reversal_flag_is_rejected() {
    LedMappingProfile profile;
    profile.segment[0].reversed = 2;

    TEST_ASSERT_FALSE(
        profile.valid());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_default_geometry_boundaries);
    RUN_TEST(test_out_of_range_is_invalid);
    RUN_TEST(test_reversed_segment_mapping);
    RUN_TEST(test_runtime_profile_can_permute_lanes);
    RUN_TEST(test_runtime_profile_reverses_only_selected_segment);
    RUN_TEST(test_disabled_pixel_follows_logical_segment_offset_under_reversal);
    RUN_TEST(test_duplicate_runtime_lane_is_rejected);
    RUN_TEST(test_invalid_runtime_reversal_flag_is_rejected);
    return UNITY_END();
}
