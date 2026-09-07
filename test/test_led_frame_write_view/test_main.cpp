#include <array>
#include <cstdint>

#include <unity.h>

#include "led/LedFrameWriteView.h"

using ambilight::LedFrameWriteView;
using ambilight::LedLaneWriteView;
using ambilight::Rgb8;

void test_lane_writer_stores_grb_order() {
    std::array<std::uint8_t, 12> buffer{};

    LedLaneWriteView lane{
        buffer.data(),
        4
    };

    TEST_ASSERT_TRUE(
        lane.valid());

    lane.writeUnchecked(
        2,
        Rgb8{
            0x11,
            0x22,
            0x33
        });

    TEST_ASSERT_EQUAL_UINT8(
        0x22,
        buffer[6]);

    TEST_ASSERT_EQUAL_UINT8(
        0x11,
        buffer[7]);

    TEST_ASSERT_EQUAL_UINT8(
        0x33,
        buffer[8]);

    TEST_ASSERT_EQUAL_UINT8(
        0,
        buffer[5]);

    TEST_ASSERT_EQUAL_UINT8(
        0,
        buffer[9]);
}

void test_frame_view_requires_all_physical_lanes() {
    std::array<
        std::array<std::uint8_t, 3>,
        ambilight::config::kParlioLaneCount>
        storage{};

    LedFrameWriteView view;

    TEST_ASSERT_FALSE(
        view.valid());

    for (std::size_t lane = 0;
         lane < view.lane.size();
         ++lane) {

        view.lane[lane] = {
            storage[lane].data(),
            1
        };
    }

    TEST_ASSERT_TRUE(
        view.valid());

    view.lane[1].grb =
        nullptr;

    TEST_ASSERT_FALSE(
        view.valid());
}

void test_write_view_can_address_full_physical_lane() {
    std::array<
        std::uint8_t,
        ambilight::config::
            kPhysicalLaneLength *
            3U>
        buffer{};

    LedLaneWriteView lane{
        buffer.data(),
        static_cast<std::uint16_t>(
            ambilight::config::
                kPhysicalLaneLength)
    };

    lane.writeUnchecked(
        static_cast<std::uint16_t>(
            ambilight::config::
                kPhysicalLaneLength -
            1U),
        Rgb8{
            255,
            128,
            64
        });

    const std::size_t base =
        (
            ambilight::config::
                kPhysicalLaneLength -
            1U
        ) *
        3U;

    TEST_ASSERT_EQUAL_UINT8(
        128,
        buffer[base + 0]);

    TEST_ASSERT_EQUAL_UINT8(
        255,
        buffer[base + 1]);

    TEST_ASSERT_EQUAL_UINT8(
        64,
        buffer[base + 2]);
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(
        test_lane_writer_stores_grb_order);

    RUN_TEST(
        test_frame_view_requires_all_physical_lanes);

    RUN_TEST(
        test_write_view_can_address_full_physical_lane);

    return UNITY_END();
}
