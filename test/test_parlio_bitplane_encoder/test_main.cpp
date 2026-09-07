#include <array>
#include <cstddef>
#include <cstdint>

#include <unity.h>

#include "ll_parlio_encoder.h"

namespace {

constexpr std::uint8_t kBit0Pattern = 0x04;
constexpr std::uint8_t kBit1Pattern = 0x06;
constexpr std::uint8_t kSamplesPerBit = 3;
constexpr std::size_t kEncodedBytesPerInputByte =
    kSamplesPerBit * 8;

using Encoded =
    std::array<
        std::uint8_t,
        kEncodedBytesPerInputByte>;

Encoded referenceEncode(
    const std::uint8_t laneValue[
        liteled_parlio::kDataWidth],
    std::uint8_t activeLaneMask) {

    Encoded output{};

    for (std::uint8_t lane = 0;
         lane <
            liteled_parlio::kDataWidth;
         ++lane) {

        const std::uint8_t laneBit =
            static_cast<std::uint8_t>(
                1U << lane);

        if ((activeLaneMask &
             laneBit) == 0) {

            continue;
        }

        const std::uint8_t value =
            laneValue[lane];

        for (int bit = 7;
             bit >= 0;
             --bit) {

            const std::uint8_t pattern =
                ((value >> bit) & 1U)
                    ? kBit1Pattern
                    : kBit0Pattern;

            const std::size_t offset =
                static_cast<std::size_t>(
                    7 - bit) *
                kSamplesPerBit;

            output[offset + 0] |=
                static_cast<std::uint8_t>(
                    ((pattern >> 2) & 1U)
                    << lane);

            output[offset + 1] |=
                static_cast<std::uint8_t>(
                    ((pattern >> 1) & 1U)
                    << lane);

            output[offset + 2] |=
                static_cast<std::uint8_t>(
                    (pattern & 1U)
                    << lane);
        }
    }

    return output;
}

Encoded optimizedEncode(
    const std::uint8_t laneValue[
        liteled_parlio::kDataWidth],
    std::uint8_t activeLaneMask) {

    Encoded output{};

    const auto plan =
        liteled_parlio::makeSamplePlan(
            kBit0Pattern,
            kBit1Pattern,
            kSamplesPerBit);

    liteled_parlio::
        initializeEncodedByte(
            output.data(),
            plan,
            activeLaneMask);

    liteled_parlio::
        encodeDynamicByte(
            output.data(),
            plan,
            activeLaneMask,
            laneValue);

    return output;
}

void assertMatchesReference(
    const std::uint8_t laneValue[
        liteled_parlio::kDataWidth],
    std::uint8_t activeLaneMask) {

    const auto reference =
        referenceEncode(
            laneValue,
            activeLaneMask);

    const auto optimized =
        optimizedEncode(
            laneValue,
            activeLaneMask);

    TEST_ASSERT_EQUAL_UINT8_ARRAY(
        reference.data(),
        optimized.data(),
        reference.size());
}

} // namespace

void test_ws2812_plan_classifies_constant_and_data_samples() {
    constexpr auto plan =
        liteled_parlio::makeSamplePlan(
            kBit0Pattern,
            kBit1Pattern,
            kSamplesPerBit);

    TEST_ASSERT_TRUE(plan.valid());

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            liteled_parlio::
                SampleMode::
                    ConstantOne),
        static_cast<int>(
            plan.mode[0]));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            liteled_parlio::
                SampleMode::
                    Data),
        static_cast<int>(
            plan.mode[1]));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            liteled_parlio::
                SampleMode::
                    ConstantZero),
        static_cast<int>(
            plan.mode[2]));

    TEST_ASSERT_EQUAL_UINT8(
        1,
        plan.dynamicCount);

    TEST_ASSERT_EQUAL_UINT8(
        1,
        plan.dynamicSample[0]);

    TEST_ASSERT_FALSE(
        plan.dynamicInverted[0]);
}

void test_black_white_and_sparse_lane_patterns_match_reference() {
    {
        const std::uint8_t values[
            liteled_parlio::kDataWidth] = {};

        assertMatchesReference(
            values,
            0x0F);
    }

    {
        const std::uint8_t values[
            liteled_parlio::kDataWidth] = {
                0xFF, 0xFF, 0xFF, 0xFF,
                0, 0, 0, 0
            };

        assertMatchesReference(
            values,
            0x0F);
    }

    {
        const std::uint8_t values[
            liteled_parlio::kDataWidth] = {
                0xAA, 0x55, 0x81, 0x18,
                0, 0, 0, 0
            };

        assertMatchesReference(
            values,
            0x0F);
    }

    {
        const std::uint8_t values[
            liteled_parlio::kDataWidth] = {
                0xAA, 0x55, 0xFF, 0x00,
                0x81, 0x18, 0x33, 0xCC
            };

        assertMatchesReference(
            values,
            0xA5);
    }
}

void test_deterministic_random_vectors_match_reference() {
    std::uint32_t state =
        0xC6A54001U;

    auto next =
        [&state]() {

            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;

            return state;
        };

    for (std::size_t iteration = 0;
         iteration < 2000;
         ++iteration) {

        std::uint8_t values[
            liteled_parlio::kDataWidth] = {};

        for (auto& value :
             values) {

            value =
                static_cast<std::uint8_t>(
                    next() & 0xFFU);
        }

        std::uint8_t mask =
            static_cast<std::uint8_t>(
                next() & 0xFFU);

        if (mask == 0) {
            mask = 1;
        }

        assertMatchesReference(
            values,
            mask);
    }
}

void test_inverted_data_sample_is_supported() {
    constexpr std::uint8_t bit0 = 0x06; // 110
    constexpr std::uint8_t bit1 = 0x04; // 100

    constexpr auto plan =
        liteled_parlio::makeSamplePlan(
            bit0,
            bit1,
            3);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            liteled_parlio::
                SampleMode::
                    InvertedData),
        static_cast<int>(
            plan.mode[1]));
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(
        test_ws2812_plan_classifies_constant_and_data_samples);

    RUN_TEST(
        test_black_white_and_sparse_lane_patterns_match_reference);

    RUN_TEST(
        test_deterministic_random_vectors_match_reference);

    RUN_TEST(
        test_inverted_data_sample_is_supported);

    return UNITY_END();
}
