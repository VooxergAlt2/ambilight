#include <unity.h>

#include "core/LatencyHistogram.h"

using ambilight::LatencyHistogram;

void test_empty_histogram_returns_zero() {
    LatencyHistogram histogram;

    TEST_ASSERT_EQUAL_UINT32(
        0,
        histogram.percentileUpperBoundUs(50));
    TEST_ASSERT_EQUAL_UINT64(0, histogram.samples());
    TEST_ASSERT_EQUAL_UINT64(0, histogram.overflow());
}

void test_percentiles_use_bucket_upper_bounds() {
    LatencyHistogram histogram;

    for (int i = 0; i < 50; ++i) {
        histogram.observe(400);
    }

    for (int i = 0; i < 45; ++i) {
        histogram.observe(3000);
    }

    for (int i = 0; i < 5; ++i) {
        histogram.observe(12000);
    }

    TEST_ASSERT_EQUAL_UINT64(100, histogram.samples());

    TEST_ASSERT_EQUAL_UINT32(
        500,
        histogram.percentileUpperBoundUs(50));

    TEST_ASSERT_EQUAL_UINT32(
        4000,
        histogram.percentileUpperBoundUs(95));

    TEST_ASSERT_EQUAL_UINT32(
        16000,
        histogram.percentileUpperBoundUs(99));

    TEST_ASSERT_EQUAL_UINT64(12000, histogram.maxObservedUs());
}

void test_overflow_is_counted_without_allocation() {
    LatencyHistogram histogram;

    histogram.observe(100);
    histogram.observe(200000);
    histogram.observe(500000);

    TEST_ASSERT_EQUAL_UINT64(3, histogram.samples());
    TEST_ASSERT_EQUAL_UINT64(2, histogram.overflow());
    TEST_ASSERT_EQUAL_UINT64(500000, histogram.maxObservedUs());

    // Percentile lands in overflow. API intentionally returns the largest
    // represented bucket bound while overflow() reveals clipping.
    TEST_ASSERT_EQUAL_UINT32(
        128000,
        histogram.percentileUpperBoundUs(100));
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_empty_histogram_returns_zero);
    RUN_TEST(test_percentiles_use_bucket_upper_bounds);
    RUN_TEST(test_overflow_is_counted_without_allocation);

    return UNITY_END();
}
