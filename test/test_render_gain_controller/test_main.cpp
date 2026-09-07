#include <cstdint>

#include <unity.h>

#include "render/RenderGainController.h"
#include "render/ShadowGainProbe.h"

using ambilight::RenderGainContext;
using ambilight::RenderGainController;
using ambilight::RenderGainControllerConfig;
using ambilight::SegmentId;
using ambilight::ShadowGainProbe;
using ambilight::kGainUnityQ12;

namespace {

RenderGainContext makeUniformTarget(
    std::uint32_t generation,
    std::uint16_t gainQ12,
    std::uint64_t timestampUs) {

    RenderGainContext context;

    context.sourceGeneration = generation;
    context.sourceTimestampUs = timestampUs;
    context.sourceAgeUs = 0;

    context.sourcePresent = true;
    context.sourceUsable = true;
    context.failOpen = false;

    const std::size_t count =
        context.topology.totalLedCount();

    for (std::size_t index = 0;
         index < count;
         ++index) {

        context.logicalGainQ12[
            index] =
            gainQ12;
    }

    return context;
}

std::uint16_t leftStart(
    const RenderGainContext& context) {

    return context.endpointsForSegment(
        SegmentId::Left).startQ12;
}

} // namespace

void test_first_valid_target_starts_from_unity() {
    RenderGainController controller;

    const auto target =
        makeUniformTarget(
            1,
            2048,
            1000000);

    TEST_ASSERT_TRUE(
        controller.setTarget(
            target,
            1000000));

    const auto& current =
        controller.current();

    TEST_ASSERT_TRUE(
        current.sourceUsable);

    TEST_ASSERT_FALSE(
        current.failOpen);

    TEST_ASSERT_EQUAL_UINT16(
        kGainUnityQ12,
        leftStart(current));

    TEST_ASSERT_FALSE(
        controller.settled());

    TEST_ASSERT_FALSE(
        controller.nonUnity());
}

void test_slew_moves_toward_target_by_real_time() {
    RenderGainControllerConfig config;
    config.slewQ12PerSecond = 8192;

    RenderGainController controller(config);

    const auto target =
        makeUniformTarget(
            1,
            2048,
            1000000);

    controller.setTarget(
        target,
        1000000);

    // 100 ms at 8192 Q12/s -> max step about 820.
    TEST_ASSERT_TRUE(
        controller.advance(
            1100000));

    TEST_ASSERT_TRUE(
        leftStart(
            controller.current()) <
        4096);

    TEST_ASSERT_TRUE(
        leftStart(
            controller.current()) >
        3200);

    TEST_ASSERT_TRUE(
        controller.nonUnity());

    // Enough additional time must reach the target exactly.
    controller.advance(
        1500000);

    TEST_ASSERT_EQUAL_UINT16(
        2048,
        leftStart(
            controller.current()));

    TEST_ASSERT_TRUE(
        controller.settled());
}

void test_fail_open_snaps_immediately_to_unity() {
    RenderGainControllerConfig config;
    config.slewQ12PerSecond = 65535;

    RenderGainController controller(config);

    const auto target =
        makeUniformTarget(
            1,
            1024,
            1000000);

    controller.setTarget(
        target,
        1000000);

    controller.advance(
        1100000);

    TEST_ASSERT_TRUE(
        leftStart(
            controller.current()) <
        kGainUnityQ12);

    RenderGainContext failOpen;
    failOpen.sourcePresent = false;
    failOpen.sourceUsable = false;
    failOpen.failOpen = true;

    TEST_ASSERT_TRUE(
        controller.setTarget(
            failOpen,
            1101000));

    const auto& recovered =
        controller.current();

    TEST_ASSERT_TRUE(
        recovered.failOpen);

    TEST_ASSERT_FALSE(
        recovered.sourceUsable);

    TEST_ASSERT_EQUAL_UINT16(
        kGainUnityQ12,
        leftStart(recovered));

    TEST_ASSERT_TRUE(
        controller.settled());

    TEST_ASSERT_FALSE(
        controller.nonUnity());

    TEST_ASSERT_EQUAL_UINT32(
        1,
        controller.stats().
            failOpenUnitySnaps);
}

void test_reacquisition_slews_from_unity() {
    RenderGainController controller;

    RenderGainContext failOpen;

    controller.setTarget(
        failOpen,
        1000000);

    const auto target =
        makeUniformTarget(
            2,
            2048,
            1100000);

    controller.setTarget(
        target,
        1100000);

    TEST_ASSERT_EQUAL_UINT16(
        kGainUnityQ12,
        leftStart(
            controller.current()));

    controller.advance(
        1200000);

    TEST_ASSERT_TRUE(
        leftStart(
            controller.current()) <
        kGainUnityQ12);
}

void test_time_rollback_resets_to_unity() {
    RenderGainController controller;

    const auto target =
        makeUniformTarget(
            1,
            2048,
            1000000);

    controller.setTarget(
        target,
        1000000);

    controller.advance(
        1100000);

    TEST_ASSERT_TRUE(
        controller.advance(
            900000));

    TEST_ASSERT_EQUAL_UINT16(
        kGainUnityQ12,
        leftStart(
            controller.current()));

    TEST_ASSERT_EQUAL_UINT32(
        1,
        controller.stats().
            timeRollbacks);
}

void test_gradient_endpoints_slew_independently() {
    RenderGainControllerConfig config;
    config.slewQ12PerSecond = 4096;

    RenderGainController controller(config);

    const auto target =
        ShadowGainProbe::make(
            123,
            1000000);

    controller.setTarget(
        target,
        1000000);

    controller.advance(
        1125000);

    const auto& after =
        controller.current();

    const auto top =
        after.endpointsForSegment(
            SegmentId::Top);

    TEST_ASSERT_EQUAL_UINT16(
        4096,
        top.startQ12);

    TEST_ASSERT_TRUE(
        top.endQ12 < 4096);

    TEST_ASSERT_TRUE(
        top.endQ12 > 3072);

    const auto bottom =
        after.endpointsForSegment(
            SegmentId::Bottom);

    TEST_ASSERT_TRUE(
        bottom.startQ12 < 4096);

    TEST_ASSERT_EQUAL_UINT16(
        4096,
        bottom.endQ12);
}

void test_same_target_profile_does_not_reopen_settled_controller() {
    RenderGainControllerConfig config;
    config.slewQ12PerSecond = 65535;

    RenderGainController controller(config);

    const auto target =
        makeUniformTarget(
            10,
            2048,
            1000000);

    controller.setTarget(
        target,
        1000000);

    TEST_ASSERT_FALSE(
        controller.settled());

    controller.advance(
        1100000);

    TEST_ASSERT_TRUE(
        controller.settled());

    auto sameProfileNewGeneration =
        makeUniformTarget(
            11,
            2048,
            1200000);

    TEST_ASSERT_FALSE(
        controller.setTarget(
            sameProfileNewGeneration,
            1200000));

    TEST_ASSERT_TRUE(
        controller.settled());

    TEST_ASSERT_EQUAL_UINT32(
        2,
        controller.stats().
            targetUpdates);

    TEST_ASSERT_EQUAL_UINT32(
        1,
        controller.stats().
            renderProfileChanges);
}

void test_advance_while_settled_is_o1_noop_semantically() {
    RenderGainController controller;

    RenderGainContext failOpen;

    controller.setTarget(
        failOpen,
        1000000);

    TEST_ASSERT_TRUE(
        controller.settled());

    TEST_ASSERT_FALSE(
        controller.advance(
            1100000));

    TEST_ASSERT_EQUAL_UINT32(
        1,
        controller.stats().
            advances);
}

void test_probe_profile_is_non_unity_and_deterministic() {
    const auto probe =
        ShadowGainProbe::make(
            77,
            5000);

    TEST_ASSERT_TRUE(
        probe.sourceUsable);

    TEST_ASSERT_TRUE(
        probe.hasNonUnityGain());

    const auto top =
        probe.endpointsForSegment(
            SegmentId::Top);

    const auto right =
        probe.endpointsForSegment(
            SegmentId::Right);

    const auto bottom =
        probe.endpointsForSegment(
            SegmentId::Bottom);

    const auto left =
        probe.endpointsForSegment(
            SegmentId::Left);

    TEST_ASSERT_EQUAL_UINT16(
        4096,
        top.startQ12);

    TEST_ASSERT_EQUAL_UINT16(
        3072,
        top.endQ12);

    TEST_ASSERT_EQUAL_UINT16(
        3072,
        right.startQ12);

    TEST_ASSERT_EQUAL_UINT16(
        3072,
        right.endQ12);

    TEST_ASSERT_EQUAL_UINT16(
        2048,
        bottom.startQ12);

    TEST_ASSERT_EQUAL_UINT16(
        4096,
        bottom.endQ12);

    TEST_ASSERT_EQUAL_UINT16(
        1024,
        left.startQ12);

    TEST_ASSERT_EQUAL_UINT16(
        1024,
        left.endQ12);
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(
        test_first_valid_target_starts_from_unity);

    RUN_TEST(
        test_slew_moves_toward_target_by_real_time);

    RUN_TEST(
        test_fail_open_snaps_immediately_to_unity);

    RUN_TEST(
        test_reacquisition_slews_from_unity);

    RUN_TEST(
        test_time_rollback_resets_to_unity);

    RUN_TEST(
        test_gradient_endpoints_slew_independently);

    RUN_TEST(
        test_same_target_profile_does_not_reopen_settled_controller);

    RUN_TEST(
        test_advance_while_settled_is_o1_noop_semantically);

    RUN_TEST(
        test_probe_profile_is_non_unity_and_deterministic);

    return UNITY_END();
}
