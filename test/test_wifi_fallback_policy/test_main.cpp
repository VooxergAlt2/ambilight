#include <unity.h>

#include <cstdint>
#include <limits>

#include "network/WifiFallbackPolicy.h"

using ambilight::WifiFallbackPolicy;

void test_fallback_waits_full_minute() {
    WifiFallbackPolicy policy;

    policy.arm(1000U);

    TEST_ASSERT_TRUE(policy.armed());
    TEST_ASSERT_FALSE(policy.due(60999U));
    TEST_ASSERT_TRUE(policy.due(61000U));
}

void test_successful_connection_can_cancel_pending_fallback() {
    WifiFallbackPolicy policy;

    policy.arm(42U);
    policy.cancel();

    TEST_ASSERT_FALSE(policy.armed());
    TEST_ASSERT_FALSE(policy.due(70000U));
}

void test_disconnect_can_start_a_fresh_grace_period() {
    WifiFallbackPolicy policy;

    policy.arm(100U);
    TEST_ASSERT_TRUE(policy.due(60100U));

    policy.arm(70000U);

    TEST_ASSERT_FALSE(policy.due(129999U));
    TEST_ASSERT_TRUE(policy.due(130000U));
}

void test_retry_after_ap_start_failure_is_shorter_than_full_grace() {
    WifiFallbackPolicy policy;

    policy.arm(0U);
    TEST_ASSERT_TRUE(policy.due(60000U));

    policy.retryLater(60000U);

    TEST_ASSERT_FALSE(policy.due(64999U));
    TEST_ASSERT_TRUE(policy.due(65000U));
}

void test_deadline_comparison_survives_millis_wrap() {
    WifiFallbackPolicy policy;

    const std::uint32_t start =
        std::numeric_limits<std::uint32_t>::max() -
        30000U;

    policy.arm(start);

    TEST_ASSERT_FALSE(
        policy.due(start + 59999U));
    TEST_ASSERT_TRUE(
        policy.due(start + 60000U));
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_fallback_waits_full_minute);
    RUN_TEST(test_successful_connection_can_cancel_pending_fallback);
    RUN_TEST(test_disconnect_can_start_a_fresh_grace_period);
    RUN_TEST(test_retry_after_ap_start_failure_is_shorter_than_full_grace);
    RUN_TEST(test_deadline_comparison_survives_millis_wrap);

    return UNITY_END();
}
