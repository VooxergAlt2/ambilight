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

void test_full_920_led_capacity_is_supported() {
    LedMappingProfile profile;

    for (auto& mapping :
         profile.segment) {

        mapping.logicalLength =
            230;
    }

    LedRenderPlan plan;

    TEST_ASSERT_TRUE(
        LedRenderPlan::build(
            profile,
            plan));

    TEST_ASSERT_EQUAL_UINT16(
        920,
        plan.totalLedCount);

    const auto& left =
        segment(
            plan,
            SegmentId::Left);

    TEST_ASSERT_EQUAL_UINT16(
        690,
        left.logicalStart);

    TEST_ASSERT_EQUAL_UINT16(
        229,
        left.physicalIndex(229));
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
}

void test_disabled_pixel_remains_logical_under_reversal() {
    LedMappingProfile profile;

    profile.segment[
        static_cast<std::size_t>(
            SegmentId::Top)].
        reversed = 1;

    LedPixelMaskProfile mask;

    mask.disabledOffset[
        static_cast<std::size_t>(
            SegmentId::Top)] = 7;

    TEST_ASSERT_TRUE(
        mask.validFor(
            profile));

    LedRenderPlan plan;

    TEST_ASSERT_TRUE(
        LedRenderPlan::build(
            profile,
            plan));

    const auto& top =
        segment(
            plan,
            SegmentId::Top);

    TEST_ASSERT_EQUAL_UINT16(
        222,
        top.physicalIndex(7));

    TEST_ASSERT_TRUE(
        mask.disabled(
            SegmentId::Top,
            7));

    TEST_ASSERT_EQUAL_UINT16(
        7,
        top.physicalIndex(222));

    TEST_ASSERT_FALSE(
        mask.disabled(
            SegmentId::Top,
            222));
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
        test_full_920_led_capacity_is_supported);

    RUN_TEST(
        test_invalid_profile_does_not_build_plan);

    RUN_TEST(
        test_disabled_pixel_remains_logical_under_reversal);

    return UNITY_END();
}
