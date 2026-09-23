#include <unity.h>

#include "led/LedPixelMaskProfile.h"
#include "led/LedRenderPlan.h"

using ambilight::LedMappingProfile;
using ambilight::LedPixelMaskProfile;
using ambilight::LedRenderPlan;
using ambilight::SegmentId;

namespace {

const ambilight::LedRenderSegment& segment(
    const LedRenderPlan& plan,
    SegmentId id) {

    return plan.segment[
        static_cast<std::size_t>(id)];
}

} // namespace

void test_default_plan_boundaries() {
    LedRenderPlan plan;

    TEST_ASSERT_TRUE(
        LedRenderPlan::build(
            LedMappingProfile{},
            plan));

    TEST_ASSERT_TRUE(plan.valid);
    TEST_ASSERT_EQUAL_UINT16(
        780,
        plan.totalLedCount);

    const auto& top =
        segment(
            plan,
            SegmentId::Top);

    TEST_ASSERT_EQUAL_UINT16(
        0,
        top.logicalStart);
    TEST_ASSERT_EQUAL_UINT16(
        230,
        top.logicalLength);
    const auto defaultTop =
        LedMappingProfile{}.
            forSegment(
                SegmentId::Top);

    TEST_ASSERT_EQUAL_UINT8(
        defaultTop.lane,
        top.lane);

    TEST_ASSERT_EQUAL_INT(
        defaultTop.reversed != 0,
        top.reversed);

    const auto& right =
        segment(
            plan,
            SegmentId::Right);

    TEST_ASSERT_EQUAL_UINT16(
        230,
        right.logicalStart);
    TEST_ASSERT_EQUAL_UINT16(
        160,
        right.logicalLength);

    const auto& bottom =
        segment(
            plan,
            SegmentId::Bottom);

    TEST_ASSERT_EQUAL_UINT16(
        390,
        bottom.logicalStart);
    TEST_ASSERT_EQUAL_UINT16(
        230,
        bottom.logicalLength);

    const auto& left =
        segment(
            plan,
            SegmentId::Left);

    TEST_ASSERT_EQUAL_UINT16(
        620,
        left.logicalStart);
    TEST_ASSERT_EQUAL_UINT16(
        160,
        left.logicalLength);
}

void test_reversed_segment_physical_indices() {
    LedMappingProfile profile;

    profile.segment[
        static_cast<std::size_t>(
            SegmentId::Top)].
        reversed = 1;

    LedRenderPlan plan;

    TEST_ASSERT_TRUE(
        LedRenderPlan::build(
            profile,
            plan));

    const auto& top =
        segment(
            plan,
            SegmentId::Top);

    TEST_ASSERT_TRUE(
        top.reversed);

    TEST_ASSERT_EQUAL_UINT16(
        229,
        top.physicalIndex(0));

    TEST_ASSERT_EQUAL_UINT16(
        0,
        top.physicalIndex(229));
}

void test_runtime_profile_can_permute_lanes() {
    LedMappingProfile profile;

    profile.segment[0].lane = 3;
    profile.segment[1].lane = 2;
    profile.segment[2].lane = 1;
    profile.segment[3].lane = 0;

    LedRenderPlan plan;

    TEST_ASSERT_TRUE(
        LedRenderPlan::build(
            profile,
            plan));

    TEST_ASSERT_EQUAL_UINT8(
        3,
        plan.segment[0].lane);
    TEST_ASSERT_EQUAL_UINT8(
        2,
        plan.segment[1].lane);
    TEST_ASSERT_EQUAL_UINT8(
        1,
        plan.segment[2].lane);
    TEST_ASSERT_EQUAL_UINT8(
        0,
        plan.segment[3].lane);
}

void test_runtime_lengths_recompute_logical_starts_once() {
    LedMappingProfile profile;

    profile.segment[0].logicalLength = 100;
    profile.segment[1].logicalLength = 50;
    profile.segment[2].logicalLength = 120;
    profile.segment[3].logicalLength = 60;

    LedRenderPlan plan;

    TEST_ASSERT_TRUE(
        LedRenderPlan::build(
            profile,
            plan));

    TEST_ASSERT_EQUAL_UINT16(
        330,
        plan.totalLedCount);

    TEST_ASSERT_EQUAL_UINT16(
        0,
        plan.segment[0].logicalStart);

    TEST_ASSERT_EQUAL_UINT16(
        100,
        plan.segment[1].logicalStart);

    TEST_ASSERT_EQUAL_UINT16(
        150,
        plan.segment[2].logicalStart);

    TEST_ASSERT_EQUAL_UINT16(
        270,
        plan.segment[3].logicalStart);
}

void test_uneven_lanes_above_230_are_supported() {
    LedMappingProfile profile;

    profile.segment[0].logicalLength = 500;
    profile.segment[1].logicalLength = 140;
    profile.segment[2].logicalLength = 140;
    profile.segment[3].logicalLength = 140;

    LedRenderPlan plan;

    TEST_ASSERT_TRUE(
        LedRenderPlan::build(
            profile,
            plan));

    TEST_ASSERT_EQUAL_UINT16(
        920,
        plan.totalLedCount);

    TEST_ASSERT_EQUAL_UINT16(
        500,
        profile.maxSegmentLength());

    const auto& right =
        segment(
            plan,
            SegmentId::Right);

    TEST_ASSERT_EQUAL_UINT16(
        500,
        right.logicalStart);

    const auto& top =
        segment(
            plan,
            SegmentId::Top);

    TEST_ASSERT_EQUAL_UINT16(
        499,
        top.physicalIndex(0));
}

void test_aggregate_logical_indices_do_not_wrap_above_uint16() {
    LedMappingProfile profile;

    for (auto& mapping : profile.segment) {
        mapping.logicalLength = 30000;
    }

    LedRenderPlan plan;

    TEST_ASSERT_TRUE(
        LedRenderPlan::build(
            profile,
            plan));

    TEST_ASSERT_EQUAL_UINT32(
        120000,
        plan.totalLedCount);

    TEST_ASSERT_EQUAL_UINT32(
        90000,
        plan.segment[3].logicalStart);

    TEST_ASSERT_EQUAL_UINT16(
        0,
        plan.segment[3].physicalIndex(0));
}

void test_invalid_profile_does_not_build_plan() {
    LedMappingProfile duplicateLane;
    duplicateLane.segment[1].lane = 0;

    LedRenderPlan plan;

    TEST_ASSERT_FALSE(
        LedRenderPlan::build(
            duplicateLane,
            plan));

    TEST_ASSERT_FALSE(plan.valid);
    TEST_ASSERT_EQUAL_UINT16(
        0,
        plan.totalLedCount);

    LedMappingProfile invalidReverse;
    invalidReverse.segment[0].reversed = 2;

    TEST_ASSERT_FALSE(
        LedRenderPlan::build(
            invalidReverse,
            plan));

    TEST_ASSERT_FALSE(plan.valid);

    LedMappingProfile largeTopology;
    largeTopology.segment[0].logicalLength = 1000;
    largeTopology.segment[1].logicalLength = 900;
    largeTopology.segment[2].logicalLength = 800;
    largeTopology.segment[3].logicalLength = 700;

    TEST_ASSERT_TRUE(
        LedRenderPlan::build(
            largeTopology,
            plan));

    TEST_ASSERT_EQUAL_UINT32(
        3400,
        plan.totalLedCount);
}

void test_disabled_pixel_is_a_physical_hole_under_reversal() {
    LedMappingProfile profile;

    profile.segment[
        static_cast<std::size_t>(
            SegmentId::Top)].reversed = 1;

    LedPixelMaskProfile mask;
    mask.disabledOffset[
        static_cast<std::size_t>(
            SegmentId::Top)] = 7;

    TEST_ASSERT_TRUE(mask.validFor(profile));

    LedRenderPlan plan;
    TEST_ASSERT_TRUE(
        LedRenderPlan::build(
            profile,
            mask,
            plan));

    const auto& top =
        segment(plan, SegmentId::Top);

    TEST_ASSERT_EQUAL_UINT16(231, top.physicalLength);
    TEST_ASSERT_EQUAL_UINT16(223, top.physicalIndex(7));
    TEST_ASSERT_EQUAL_UINT16(8, top.physicalIndex(222));
    TEST_ASSERT_EQUAL_UINT16(6, top.physicalIndex(223));
    TEST_ASSERT_TRUE(mask.disabledPhysical(SegmentId::Top, 7));

    // No logical LED is ever mapped onto the disabled physical address.
    for (std::uint16_t logical = 0;
         logical < top.logicalLength;
         ++logical) {

        TEST_ASSERT_NOT_EQUAL(
            7,
            top.physicalIndex(logical));
    }
}


void test_physical_hole_shifts_forward_mapping_without_losing_logical_pixel() {
    LedMappingProfile profile;
    profile.segment[0].logicalLength = 3;
    profile.segment[0].reversed = 0;

    LedPixelMaskProfile mask;
    mask.disabledOffset[0] = 0;

    LedRenderPlan plan;
    TEST_ASSERT_TRUE(LedRenderPlan::build(profile, mask, plan));

    const auto& top = plan.segment[0];
    TEST_ASSERT_EQUAL_UINT16(4, top.physicalLength);
    TEST_ASSERT_EQUAL_UINT16(0, top.disabledPhysicalOffset);
    TEST_ASSERT_EQUAL_UINT16(1, top.physicalIndex(0));
    TEST_ASSERT_EQUAL_UINT16(2, top.physicalIndex(1));
    TEST_ASSERT_EQUAL_UINT16(3, top.physicalIndex(2));
}

void test_physical_hole_shifts_reversed_mapping_around_data_side_pixel() {
    LedMappingProfile profile;
    profile.segment[0].logicalLength = 3;
    profile.segment[0].reversed = 1;

    LedPixelMaskProfile mask;
    mask.disabledOffset[0] = 0;

    LedRenderPlan plan;
    TEST_ASSERT_TRUE(LedRenderPlan::build(profile, mask, plan));

    const auto& top = plan.segment[0];
    TEST_ASSERT_EQUAL_UINT16(4, top.physicalLength);
    TEST_ASSERT_EQUAL_UINT16(3, top.physicalIndex(0));
    TEST_ASSERT_EQUAL_UINT16(2, top.physicalIndex(1));
    TEST_ASSERT_EQUAL_UINT16(1, top.physicalIndex(2));
}

void test_physical_hole_can_be_inside_or_after_logical_pixels() {
    LedMappingProfile profile;
    profile.segment[0].logicalLength = 3;
    profile.segment[0].reversed = 0;

    LedPixelMaskProfile mask;
    mask.disabledOffset[0] = 1;

    LedRenderPlan plan;
    TEST_ASSERT_TRUE(LedRenderPlan::build(profile, mask, plan));
    TEST_ASSERT_EQUAL_UINT16(0, plan.segment[0].physicalIndex(0));
    TEST_ASSERT_EQUAL_UINT16(2, plan.segment[0].physicalIndex(1));
    TEST_ASSERT_EQUAL_UINT16(3, plan.segment[0].physicalIndex(2));

    mask.disabledOffset[0] = 3;
    TEST_ASSERT_TRUE(mask.validFor(profile));
    TEST_ASSERT_TRUE(LedRenderPlan::build(profile, mask, plan));
    TEST_ASSERT_EQUAL_UINT16(4, plan.segment[0].physicalLength);
    TEST_ASSERT_EQUAL_UINT16(0, plan.segment[0].physicalIndex(0));
    TEST_ASSERT_EQUAL_UINT16(1, plan.segment[0].physicalIndex(1));
    TEST_ASSERT_EQUAL_UINT16(2, plan.segment[0].physicalIndex(2));
}

void test_mask_grows_max_physical_lane_and_rejects_uint16_overflow() {
    LedMappingProfile profile;
    profile.segment[0].logicalLength = 230;
    profile.segment[1].logicalLength = 160;
    profile.segment[2].logicalLength = 230;
    profile.segment[3].logicalLength = 160;

    LedPixelMaskProfile mask;
    mask.disabledOffset[0] = 0;

    TEST_ASSERT_TRUE(mask.validFor(profile));
    TEST_ASSERT_EQUAL_UINT32(231, mask.maxPhysicalLaneLength(profile));

    profile.segment[0].logicalLength = 65535;
    TEST_ASSERT_FALSE(mask.validFor(profile));
    TEST_ASSERT_EQUAL_UINT32(
        0,
        mask.maxPhysicalLaneLength(profile));
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(
        test_default_plan_boundaries);

    RUN_TEST(
        test_reversed_segment_physical_indices);

    RUN_TEST(
        test_runtime_profile_can_permute_lanes);

    RUN_TEST(
        test_runtime_lengths_recompute_logical_starts_once);

    RUN_TEST(
        test_uneven_lanes_above_230_are_supported);

    RUN_TEST(
        test_aggregate_logical_indices_do_not_wrap_above_uint16);

    RUN_TEST(
        test_invalid_profile_does_not_build_plan);

    RUN_TEST(
        test_disabled_pixel_is_a_physical_hole_under_reversal);

    RUN_TEST(test_physical_hole_shifts_forward_mapping_without_losing_logical_pixel);
    RUN_TEST(test_physical_hole_shifts_reversed_mapping_around_data_side_pixel);
    RUN_TEST(test_physical_hole_can_be_inside_or_after_logical_pixels);
    RUN_TEST(test_mask_grows_max_physical_lane_and_rejects_uint16_overflow);

    return UNITY_END();
}
