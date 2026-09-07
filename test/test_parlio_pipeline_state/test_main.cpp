#include <cstdint>

#include <unity.h>

#include "ll_parlio_pipeline.h"

using Pipeline =
    liteled_parlio::PipelineState<2>;

void test_cold_start_encodes_buffer_zero() {
    Pipeline state;

    TEST_ASSERT_TRUE(
        state.canEncode());

    TEST_ASSERT_EQUAL_UINT8(
        0,
        state.nextEncodeBuffer());

    TEST_ASSERT_FALSE(
        state.inFlight());

    TEST_ASSERT_FALSE(
        state.encodedReady());
}

void test_submit_moves_buffer_zero_in_flight_and_frees_one() {
    Pipeline state;

    state.markEncoded();

    TEST_ASSERT_TRUE(
        state.encodedReady());

    TEST_ASSERT_EQUAL_UINT8(
        0,
        state.readyBuffer());

    TEST_ASSERT_TRUE(
        state.canTransmit());

    state.markTransmitted();

    TEST_ASSERT_TRUE(
        state.inFlight());

    TEST_ASSERT_EQUAL_UINT8(
        0,
        state.inFlightBuffer());

    TEST_ASSERT_EQUAL_UINT8(
        1,
        state.nextEncodeBuffer());

    TEST_ASSERT_TRUE(
        state.canEncode());
}

void test_next_frame_can_encode_while_previous_is_in_flight() {
    Pipeline state;

    state.markEncoded();
    state.markTransmitted();

    TEST_ASSERT_TRUE(
        state.inFlight());

    TEST_ASSERT_TRUE(
        state.canEncode());

    TEST_ASSERT_EQUAL_UINT8(
        1,
        state.nextEncodeBuffer());

    state.markEncoded();

    TEST_ASSERT_TRUE(
        state.encodedReady());

    TEST_ASSERT_EQUAL_UINT8(
        1,
        state.readyBuffer());

    TEST_ASSERT_FALSE(
        state.canTransmit());

    TEST_ASSERT_FALSE(
        state.canEncode());
}

void test_wait_releases_old_buffer_then_ready_frame_can_transmit() {
    Pipeline state;

    state.markEncoded();
    state.markTransmitted();

    state.markEncoded();

    TEST_ASSERT_TRUE(
        state.canWait());

    state.markWaited();

    TEST_ASSERT_FALSE(
        state.inFlight());

    TEST_ASSERT_TRUE(
        state.canTransmit());

    state.markTransmitted();

    TEST_ASSERT_EQUAL_UINT8(
        1,
        state.inFlightBuffer());

    TEST_ASSERT_EQUAL_UINT8(
        0,
        state.nextEncodeBuffer());
}

void test_buffers_alternate_without_aliasing_dma_owned_buffer() {
    Pipeline state;

    for (std::uint8_t frame = 0;
         frame < 20;
         ++frame) {

        TEST_ASSERT_TRUE(
            state.canEncode());

        const std::uint8_t encoded =
            state.nextEncodeBuffer();

        if (state.inFlight()) {
            TEST_ASSERT_NOT_EQUAL(
                state.inFlightBuffer(),
                encoded);
        }

        state.markEncoded();

        if (state.inFlight()) {
            TEST_ASSERT_TRUE(
                state.canWait());

            state.markWaited();
        }

        TEST_ASSERT_TRUE(
            state.canTransmit());

        state.markTransmitted();

        TEST_ASSERT_EQUAL_UINT8(
            encoded,
            state.inFlightBuffer());
    }
}

void test_reset_returns_to_cold_start() {
    Pipeline state;

    state.markEncoded();
    state.markTransmitted();
    state.markEncoded();

    state.reset();

    TEST_ASSERT_FALSE(
        state.inFlight());

    TEST_ASSERT_FALSE(
        state.encodedReady());

    TEST_ASSERT_TRUE(
        state.canEncode());

    TEST_ASSERT_EQUAL_UINT8(
        0,
        state.nextEncodeBuffer());
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(
        test_cold_start_encodes_buffer_zero);

    RUN_TEST(
        test_submit_moves_buffer_zero_in_flight_and_frees_one);

    RUN_TEST(
        test_next_frame_can_encode_while_previous_is_in_flight);

    RUN_TEST(
        test_wait_releases_old_buffer_then_ready_frame_can_transmit);

    RUN_TEST(
        test_buffers_alternate_without_aliasing_dma_owned_buffer);

    RUN_TEST(
        test_reset_returns_to_cold_start);

    return UNITY_END();
}
