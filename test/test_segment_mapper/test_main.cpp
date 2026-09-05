#include <unity.h>

#include "core/Geometry.h"
#include "led/SegmentMapper.h"

using ambilight::PhysicalPixel;
using ambilight::SegmentConfig;
using ambilight::SegmentId;
using ambilight::SegmentMapper;

void test_default_geometry_boundaries() {
    const PhysicalPixel p0 = SegmentMapper::map(0);
    TEST_ASSERT_TRUE(p0.valid);
    TEST_ASSERT_EQUAL_UINT8(0, p0.lane);
    TEST_ASSERT_EQUAL_UINT16(0, p0.index);

    const PhysicalPixel p229 = SegmentMapper::map(229);
    TEST_ASSERT_TRUE(p229.valid);
    TEST_ASSERT_EQUAL_UINT8(0, p229.lane);
    TEST_ASSERT_EQUAL_UINT16(229, p229.index);

    const PhysicalPixel p230 = SegmentMapper::map(230);
    TEST_ASSERT_TRUE(p230.valid);
    TEST_ASSERT_EQUAL_UINT8(1, p230.lane);
    TEST_ASSERT_EQUAL_UINT16(0, p230.index);

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

    const PhysicalPixel last = SegmentMapper::mapInSegment(reversed, 109);
    TEST_ASSERT_TRUE(last.valid);
    TEST_ASSERT_EQUAL_UINT8(2, last.lane);
    TEST_ASSERT_EQUAL_UINT16(0, last.index);

    const PhysicalPixel before = SegmentMapper::mapInSegment(reversed, 99);
    TEST_ASSERT_FALSE(before.valid);

    const PhysicalPixel after = SegmentMapper::mapInSegment(reversed, 110);
    TEST_ASSERT_FALSE(after.valid);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_default_geometry_boundaries);
    RUN_TEST(test_out_of_range_is_invalid);
    RUN_TEST(test_reversed_segment_mapping);
    return UNITY_END();
}
