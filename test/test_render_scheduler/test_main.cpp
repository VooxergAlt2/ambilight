#include <unity.h>

#include "render/RenderScheduler.h"

using ambilight::RenderScheduler;
using ambilight::RenderSchedulerConfig;

void test_no_rgb_frame_never_renders() {
    RenderScheduler scheduler;

    const auto decision =
        scheduler.decide(
            false,
            true,
            true,
            1000);

    TEST_ASSERT_FALSE(decision.render);
    TEST_ASSERT_EQUAL_UINT32(
        1,
        scheduler.stats().noFrameSkips);
}

void test_new_rgb_renders_immediately() {
    RenderScheduler scheduler;

    auto first =
        scheduler.decide(
            true,
            true,
            false,
            1000);

    TEST_ASSERT_TRUE(first.render);
    TEST_ASSERT_TRUE(first.dueToRgb);
    TEST_ASSERT_FALSE(first.dueToState);

    scheduler.markRendered(1000);

    // Even inside state-only rate limit, a fresh RGB frame must not wait.
    auto second =
        scheduler.decide(
            true,
            true,
            false,
            2000);

    TEST_ASSERT_TRUE(second.render);
    TEST_ASSERT_TRUE(second.dueToRgb);
}

void test_state_only_render_is_rate_limited() {
    RenderSchedulerConfig config;
    config.stateOnlyMinIntervalUs = 16667;

    RenderScheduler scheduler(config);

    auto first =
        scheduler.decide(
            true,
            false,
            true,
            1000);

    TEST_ASSERT_TRUE(first.render);
    TEST_ASSERT_TRUE(first.dueToState);

    scheduler.markRendered(1000);

    auto tooSoon =
        scheduler.decide(
            true,
            false,
            true,
            17000);

    TEST_ASSERT_FALSE(tooSoon.render);
    TEST_ASSERT_TRUE(tooSoon.stateDeferred);

    auto allowed =
        scheduler.decide(
            true,
            false,
            true,
            17667);

    TEST_ASSERT_TRUE(allowed.render);
    TEST_ASSERT_TRUE(allowed.dueToState);
}

void test_clean_state_does_not_rerender_static_rgb() {
    RenderScheduler scheduler;

    const auto decision =
        scheduler.decide(
            true,
            false,
            false,
            1000);

    TEST_ASSERT_FALSE(decision.render);
    TEST_ASSERT_EQUAL_UINT32(
        1,
        scheduler.stats().cleanSkips);
}

void test_rgb_and_state_dirty_are_combined_in_one_render() {
    RenderScheduler scheduler;

    const auto decision =
        scheduler.decide(
            true,
            true,
            true,
            1000);

    TEST_ASSERT_TRUE(decision.render);
    TEST_ASSERT_TRUE(decision.dueToRgb);
    TEST_ASSERT_TRUE(decision.dueToState);

    TEST_ASSERT_EQUAL_UINT32(
        1,
        scheduler.stats().combinedRenders);
}

void test_time_rollback_allows_state_recovery_render() {
    RenderScheduler scheduler;

    auto first =
        scheduler.decide(
            true,
            false,
            true,
            10000);

    TEST_ASSERT_TRUE(first.render);
    scheduler.markRendered(10000);

    auto rollback =
        scheduler.decide(
            true,
            false,
            true,
            9000);

    TEST_ASSERT_TRUE(rollback.render);
    TEST_ASSERT_TRUE(rollback.dueToState);

    TEST_ASSERT_EQUAL_UINT32(
        1,
        scheduler.stats().timeRollbacks);
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_no_rgb_frame_never_renders);
    RUN_TEST(test_new_rgb_renders_immediately);
    RUN_TEST(test_state_only_render_is_rate_limited);
    RUN_TEST(test_clean_state_does_not_rerender_static_rgb);
    RUN_TEST(test_rgb_and_state_dirty_are_combined_in_one_render);
    RUN_TEST(test_time_rollback_allows_state_recovery_render);

    return UNITY_END();
}
