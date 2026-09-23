#include <unity.h>

#include "led/LedRenderPlan.h"
#include "render/RenderDiagnostics.h"

using ambilight::CorrectionMode;
using ambilight::LedMappingProfile;
using ambilight::LedPixelMaskProfile;
using ambilight::LedRenderPlan;
using ambilight::RenderDiagnostics;
using ambilight::RenderGainContext;
using ambilight::Rgb8;
using ambilight::RgbFrame;
using ambilight::SegmentId;

namespace {

RenderGainContext halfGainContext() {
    RenderGainContext context;
    context.sourcePresent = true;
    context.sourceUsable = true;
    context.failOpen = false;

    const std::size_t count =
        context.topology.totalLedCount();

    for (std::size_t index = 0;
         index < count;
         ++index) {

        context.logicalGainQ12[index] =
            2048;
    }

    return context;
}

RgbFrame whiteFrame() {
    RgbFrame frame;

    for (std::size_t index = 0;
         index < frame.pixelCount;
         ++index) {

        frame.pixels[index] =
            Rgb8{100, 80, 60};
    }

    return frame;
}

} // namespace

void test_disabled_mode_is_not_analyzed() {
    LedRenderPlan plan;
    TEST_ASSERT_TRUE(
        LedRenderPlan::build(
            LedMappingProfile{},
            plan));

    RenderDiagnostics diagnostics;
    LedPixelMaskProfile mask;

    TEST_ASSERT_FALSE(
        diagnostics.analyze(
            whiteFrame(),
            halfGainContext(),
            CorrectionMode::Disabled,
            plan,
            mask));

    TEST_ASSERT_EQUAL_UINT32(
        0,
        diagnostics.stats().
            diagnosticFrames);
}

void test_shadow_reports_candidate_change_without_physical_gain_change() {
    LedRenderPlan plan;
    TEST_ASSERT_TRUE(
        LedRenderPlan::build(
            LedMappingProfile{},
            plan));

    RenderDiagnostics diagnostics;
    LedPixelMaskProfile mask;

    TEST_ASSERT_TRUE(
        diagnostics.analyze(
            whiteFrame(),
            halfGainContext(),
            CorrectionMode::Shadow,
            plan,
            mask));

    const auto& stats =
        diagnostics.stats();

    TEST_ASSERT_EQUAL_UINT32(
        1,
        stats.diagnosticFrames);

    TEST_ASSERT_EQUAL_UINT32(
        1,
        stats.shadowFrames);

    TEST_ASSERT_EQUAL_UINT16(
        780,
        stats.lastWouldChangePixels);

    TEST_ASSERT_EQUAL_UINT16(
        0,
        stats.lastPhysicalChangedPixels);

    TEST_ASSERT_TRUE(
        stats.lastCandidateChannelSum <
        stats.lastInputChannelSum);
}

void test_active_reports_gain_as_physical_change() {
    LedRenderPlan plan;
    TEST_ASSERT_TRUE(
        LedRenderPlan::build(
            LedMappingProfile{},
            plan));

    RenderDiagnostics diagnostics;
    LedPixelMaskProfile mask;

    TEST_ASSERT_TRUE(
        diagnostics.analyze(
            whiteFrame(),
            halfGainContext(),
            CorrectionMode::Active,
            plan,
            mask));

    const auto& stats =
        diagnostics.stats();

    TEST_ASSERT_EQUAL_UINT32(
        1,
        stats.activeFrames);

    TEST_ASSERT_EQUAL_UINT16(
        780,
        stats.lastWouldChangePixels);

    TEST_ASSERT_EQUAL_UINT16(
        780,
        stats.lastPhysicalChangedPixels);
}

void test_shadow_physical_hole_does_not_count_as_logical_pixel_change() {
    LedMappingProfile topology;

    LedRenderPlan plan;
    TEST_ASSERT_TRUE(
        LedRenderPlan::build(
            topology,
            plan));

    LedPixelMaskProfile mask;
    mask.disabledOffset[
        static_cast<std::size_t>(
            SegmentId::Top)] = 7;

    TEST_ASSERT_TRUE(
        mask.validFor(
            topology));

    RenderDiagnostics diagnostics;

    TEST_ASSERT_TRUE(
        diagnostics.analyze(
            whiteFrame(),
            halfGainContext(),
            CorrectionMode::Shadow,
            plan,
            mask));

    TEST_ASSERT_EQUAL_UINT16(
        0,
        diagnostics.stats().
            lastPhysicalChangedPixels);
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(
        test_disabled_mode_is_not_analyzed);

    RUN_TEST(
        test_shadow_reports_candidate_change_without_physical_gain_change);

    RUN_TEST(
        test_active_reports_gain_as_physical_change);

    RUN_TEST(
        test_shadow_physical_hole_does_not_count_as_logical_pixel_change);

    return UNITY_END();
}
