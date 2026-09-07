#include <unity.h>

#include "core/PerformanceMetric.h"

using ambilight::PerformanceMetric;

void test_empty_metric_reports_zeroes() {
    PerformanceMetric metric;

    TEST_ASSERT_EQUAL_UINT64(0, metric.samples());
    TEST_ASSERT_EQUAL_UINT64(0, metric.lastUs());
    TEST_ASSERT_EQUAL_UINT64(0, metric.maxUs());
    TEST_ASSERT_EQUAL_UINT64(0, metric.meanUs());
    TEST_ASSERT_EQUAL_UINT32(
        0,
        metric.percentileUpperBoundUs(95));
}

void test_metric_tracks_exact_last_mean_and_max() {
    PerformanceMetric metric;

    metric.observe(100);
    metric.observe(300);
    metric.observe(500);

    TEST_ASSERT_EQUAL_UINT64(3, metric.samples());
    TEST_ASSERT_EQUAL_UINT64(500, metric.lastUs());
    TEST_ASSERT_EQUAL_UINT64(500, metric.maxUs());
    TEST_ASSERT_EQUAL_UINT64(300, metric.meanUs());

    TEST_ASSERT_EQUAL_UINT32(
        500,
        metric.percentileUpperBoundUs(50));

    TEST_ASSERT_EQUAL_UINT32(
        500,
        metric.percentileUpperBoundUs(95));
}

void test_reset_clears_metric_state() {
    PerformanceMetric metric;

    metric.observe(2000);
    metric.observe(4000);
    metric.reset();

    TEST_ASSERT_EQUAL_UINT64(0, metric.samples());
    TEST_ASSERT_EQUAL_UINT64(0, metric.lastUs());
    TEST_ASSERT_EQUAL_UINT64(0, metric.maxUs());
    TEST_ASSERT_EQUAL_UINT64(0, metric.meanUs());
    TEST_ASSERT_EQUAL_UINT64(
        0,
        metric.percentileOverflow());
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_empty_metric_reports_zeroes);
    RUN_TEST(test_metric_tracks_exact_last_mean_and_max);
    RUN_TEST(test_reset_clears_metric_state);

    return UNITY_END();
}
