#include <cstdint>

#include <unity.h>

#include "integration/TofRenderGainBridge.h"
#include "led/SegmentMapper.h"
#include "render/RenderGainContext.h"

using ambilight::GainSnapshot;
using ambilight::PerimeterGainSnapshot;
using ambilight::RenderGainContext;
using ambilight::RenderGainMath;
using ambilight::Rgb8;
using ambilight::SegmentId;
using ambilight::SegmentMapper;
using ambilight::TofRenderGainBridge;
using ambilight::applyGainQ12;
using ambilight::kGainUnityQ12;

void test_q12_channel_scaling_rounds_and_clamps() {
    TEST_ASSERT_EQUAL_UINT8(
        255,
        applyGainQ12(255, 5000));

    TEST_ASSERT_EQUAL_UINT8(
        128,
        applyGainQ12(255, 2048));

    TEST_ASSERT_EQUAL_UINT8(
        1,
        applyGainQ12(1, 2048));

    TEST_ASSERT_EQUAL_UINT8(
        0,
        applyGainQ12(255, 0));
}

void test_fail_open_context_is_always_unity() {
    RenderGainContext context;
    context.sourcePresent = true;
    context.sourceUsable = false;
    context.failOpen = true;

    context.logicalGainQ12[0] = 1000;
    context.logicalGainQ12[229] = 2000;

    TEST_ASSERT_EQUAL_UINT16(
        kGainUnityQ12,
        context.gainForPosition(
            SegmentId::Top,
            0,
            230));

    TEST_ASSERT_FALSE(
        context.hasNonUnityGain());
}

void test_logical_gain_field_is_authoritative() {
    RenderGainContext context;
    context.sourcePresent = true;
    context.sourceUsable = true;
    context.failOpen = false;

    context.logicalGainQ12[0] = 4096;
    context.logicalGainQ12[1] = 3584;
    context.logicalGainQ12[2] = 3072;
    context.logicalGainQ12[3] = 2560;
    context.logicalGainQ12[4] = 2048;

    TEST_ASSERT_EQUAL_UINT16(
        4096,
        context.gainForPosition(
            SegmentId::Top,
            0,
            230));

    TEST_ASSERT_EQUAL_UINT16(
        3584,
        context.gainForPosition(
            SegmentId::Top,
            1,
            230));

    TEST_ASSERT_EQUAL_UINT16(
        3072,
        context.gainForLogicalIndex(2));

    TEST_ASSERT_EQUAL_UINT16(
        2048,
        context.gainForPosition(
            SegmentId::Top,
            4,
            230));
}

void test_shadow_preview_changes_rgb_but_not_original_value() {
    RenderGainContext context;
    context.sourcePresent = true;
    context.sourceUsable = true;
    context.failOpen = false;

    context.setSegmentUniform(
        SegmentId::Left,
        2048);

    constexpr Rgb8 input{200, 100, 50};

    const auto preview =
        RenderGainMath::preview(
            input,
            620,
            SegmentId::Left,
            context);

    TEST_ASSERT_TRUE(preview.wouldChange);

    TEST_ASSERT_EQUAL_UINT8(
        200,
        preview.original.r);

    TEST_ASSERT_EQUAL_UINT8(
        100,
        preview.wouldOutput.r);

    TEST_ASSERT_EQUAL_UINT8(
        50,
        preview.wouldOutput.g);

    TEST_ASSERT_EQUAL_UINT8(
        25,
        preview.wouldOutput.b);

    TEST_ASSERT_EQUAL_UINT8(
        100,
        preview.maxChannelDelta);
}

void test_shadow_policy_never_applies_candidate() {
    constexpr Rgb8 original{200, 100, 50};

    ambilight::ShadowPixelResult shadow;
    shadow.original = original;
    shadow.wouldOutput = Rgb8{10, 20, 30};
    shadow.wouldChange = true;

    const Rgb8 physical =
        ambilight::ShadowRenderPolicy::physicalOutput(
            original,
            shadow);

    TEST_ASSERT_EQUAL_UINT8(200, physical.r);
    TEST_ASSERT_EQUAL_UINT8(100, physical.g);
    TEST_ASSERT_EQUAL_UINT8(50, physical.b);
}

void test_render_profile_comparison_ignores_metadata_but_not_usability() {
    RenderGainContext a;
    a.sourcePresent = true;
    a.sourceUsable = true;
    a.failOpen = false;
    a.sourceGeneration = 1;
    a.sourceTimestampUs = 1000;

    RenderGainContext b = a;
    b.sourceGeneration = 999;
    b.sourceTimestampUs = 999999;
    b.sourceAgeUs = 12345;

    TEST_ASSERT_TRUE(
        a.sameRenderProfileAs(b));

    b.sourceUsable = false;
    b.failOpen = true;

    TEST_ASSERT_FALSE(
        a.sameRenderProfileAs(b));

    RenderGainContext failA;
    RenderGainContext failB;

    failA.logicalGainQ12[620] = 1000;
    failA.logicalGainQ12[779] = 2000;

    // Both are fail-open, so hidden endpoint contents cannot make RGB dirty.
    TEST_ASSERT_TRUE(
        failA.sameRenderProfileAs(failB));
}

void test_spatial_bridge_preserves_segment_endpoints() {
    PerimeterGainSnapshot gains;
    gains.generation = 77;
    gains.timestampUs = 1000000;
    gains.planeUsable = true;
    gains.projectionUsable = true;
    gains.failOpen = false;

    gains.segment[
        static_cast<std::size_t>(SegmentId::Top)] = {
            500,
            700,
            2048,
            3072
        };

    gains.segment[
        static_cast<std::size_t>(SegmentId::Right)] = {
            700,
            550,
            3072,
            2304
        };

    gains.segment[
        static_cast<std::size_t>(SegmentId::Bottom)] = {
            550,
            450,
            2304,
            1792
        };

    gains.segment[
        static_cast<std::size_t>(SegmentId::Left)] = {
            450,
            500,
            1792,
            2048
        };

    gains.logicalGainQ12.fill(
        kGainUnityQ12);

    gains.logicalGainQ12[0] = 2048;
    gains.logicalGainQ12[229] = 3072;
    gains.logicalGainQ12[230] = 3072;
    gains.logicalGainQ12[389] = 2304;
    gains.logicalGainQ12[390] = 2304;
    gains.logicalGainQ12[619] = 1792;
    gains.logicalGainQ12[620] = 1792;
    gains.logicalGainQ12[779] = 2048;

    const auto context =
        TofRenderGainBridge::make(
            gains,
            true,
            1100000);

    TEST_ASSERT_TRUE(context.sourceUsable);
    TEST_ASSERT_FALSE(context.failOpen);

    const auto top =
        context.endpointsForSegment(
            SegmentId::Top);

    const auto right =
        context.endpointsForSegment(
            SegmentId::Right);

    TEST_ASSERT_EQUAL_UINT16(
        2048,
        top.startQ12);

    TEST_ASSERT_EQUAL_UINT16(
        3072,
        top.endQ12);

    TEST_ASSERT_EQUAL_UINT16(
        3072,
        right.startQ12);

    TEST_ASSERT_EQUAL_UINT16(
        2304,
        right.endQ12);
}

void test_spatial_bridge_stale_snapshot_fails_open() {
    PerimeterGainSnapshot gains;
    gains.generation = 1;
    gains.timestampUs = 1000000;
    gains.planeUsable = true;
    gains.projectionUsable = true;
    gains.failOpen = false;

    gains.logicalGainQ12.fill(
        kGainUnityQ12);
    gains.logicalGainQ12[620] = 1000;

    const auto context =
        TofRenderGainBridge::make(
            gains,
            true,
            1000000 +
                TofRenderGainBridge::
                    kMaxGainSnapshotAgeUs +
                1);

    TEST_ASSERT_FALSE(context.sourceUsable);
    TEST_ASSERT_TRUE(context.failOpen);

    TEST_ASSERT_EQUAL_UINT16(
        kGainUnityQ12,
        context.gainForPosition(
            SegmentId::Left,
            0,
            160));
}

void test_tof_bridge_maps_four_uniform_side_gains() {
    GainSnapshot gains;
    gains.generation = 42;
    gains.timestampUs = 1000000;
    gains.geometryUsable = true;
    gains.failOpen = false;

    gains.topQ12 = 3900;
    gains.rightQ12 = 3800;
    gains.bottomQ12 = 3700;
    gains.leftQ12 = 3600;

    const RenderGainContext context =
        TofRenderGainBridge::make(
            gains,
            true,
            1100000);

    TEST_ASSERT_TRUE(context.sourcePresent);
    TEST_ASSERT_TRUE(context.sourceUsable);
    TEST_ASSERT_FALSE(context.failOpen);

    TEST_ASSERT_EQUAL_UINT32(
        42,
        context.sourceGeneration);

    TEST_ASSERT_EQUAL_UINT64(
        100000,
        context.sourceAgeUs);

    const auto top =
        context.endpointsForSegment(
            SegmentId::Top);
    const auto right =
        context.endpointsForSegment(
            SegmentId::Right);
    const auto bottom =
        context.endpointsForSegment(
            SegmentId::Bottom);
    const auto left =
        context.endpointsForSegment(
            SegmentId::Left);

    TEST_ASSERT_EQUAL_UINT16(3900, top.startQ12);
    TEST_ASSERT_EQUAL_UINT16(3900, top.endQ12);

    TEST_ASSERT_EQUAL_UINT16(3800, right.startQ12);
    TEST_ASSERT_EQUAL_UINT16(3700, bottom.startQ12);
    TEST_ASSERT_EQUAL_UINT16(3600, left.startQ12);
}

void test_tof_bridge_stale_or_future_snapshot_fails_open() {
    GainSnapshot gains;
    gains.generation = 1;
    gains.timestampUs = 1000000;
    gains.geometryUsable = true;
    gains.failOpen = false;
    gains.leftQ12 = 2000;

    auto stale =
        TofRenderGainBridge::make(
            gains,
            true,
            1000000 +
                TofRenderGainBridge::
                    kMaxGainSnapshotAgeUs +
                1);

    TEST_ASSERT_FALSE(stale.sourceUsable);
    TEST_ASSERT_TRUE(stale.failOpen);

    auto future =
        TofRenderGainBridge::make(
            gains,
            true,
            999999);

    TEST_ASSERT_FALSE(future.sourceUsable);
    TEST_ASSERT_TRUE(future.failOpen);
}

void test_bridge_clamps_gain_above_unity() {
    GainSnapshot gains;
    gains.generation = 1;
    gains.timestampUs = 1000;
    gains.geometryUsable = true;
    gains.failOpen = false;
    gains.leftQ12 = 6000;

    const auto context =
        TofRenderGainBridge::make(
            gains,
            true,
            1100);

    const auto left =
        context.endpointsForSegment(
            SegmentId::Left);

    TEST_ASSERT_EQUAL_UINT16(
        kGainUnityQ12,
        left.startQ12);
}

void test_reversed_physical_mapping_keeps_logical_offset() {
    constexpr ambilight::SegmentConfig reversed{
        SegmentId::Bottom,
        100,
        10,
        2,
        true
    };

    const auto first =
        SegmentMapper::mapInSegment(
            reversed,
            100);

    TEST_ASSERT_TRUE(first.valid);
    TEST_ASSERT_EQUAL_UINT16(9, first.index);
    TEST_ASSERT_EQUAL_UINT16(0, first.segmentOffset);
    TEST_ASSERT_EQUAL_UINT16(10, first.segmentLength);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(
            SegmentId::Bottom),
        static_cast<std::uint8_t>(
            first.segment));

    const auto last =
        SegmentMapper::mapInSegment(
            reversed,
            109);

    TEST_ASSERT_EQUAL_UINT16(0, last.index);
    TEST_ASSERT_EQUAL_UINT16(9, last.segmentOffset);
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_q12_channel_scaling_rounds_and_clamps);
    RUN_TEST(test_fail_open_context_is_always_unity);
    RUN_TEST(test_logical_gain_field_is_authoritative);
    RUN_TEST(test_shadow_preview_changes_rgb_but_not_original_value);
    RUN_TEST(test_shadow_policy_never_applies_candidate);
    RUN_TEST(test_render_profile_comparison_ignores_metadata_but_not_usability);
    RUN_TEST(test_spatial_bridge_preserves_segment_endpoints);
    RUN_TEST(test_spatial_bridge_stale_snapshot_fails_open);
    RUN_TEST(test_tof_bridge_maps_four_uniform_side_gains);
    RUN_TEST(test_tof_bridge_stale_or_future_snapshot_fails_open);
    RUN_TEST(test_bridge_clamps_gain_above_unity);
    RUN_TEST(test_reversed_physical_mapping_keeps_logical_offset);

    return UNITY_END();
}
