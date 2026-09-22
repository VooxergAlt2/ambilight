#include <array>
#include <cstdint>

#include <unity.h>

#include "led/LedPhysicalPixelMask.h"

using ambilight::LedFrameWriteView;
using ambilight::LedMappingProfile;
using ambilight::LedPhysicalPixelMask;
using ambilight::LedPixelMaskProfile;
using ambilight::Rgb8;
using ambilight::SegmentId;

namespace {

struct Fixture {
    std::array<
        std::array<std::uint8_t, ambilight::config::kTestedPhysicalLaneLength * 3U>,
        ambilight::config::kParlioLaneCount>
        bytes{};

    LedFrameWriteView frame{};

    Fixture() {
        for (std::size_t lane = 0;
             lane < frame.lane.size();
             ++lane) {

            frame.lane[lane] = {
                bytes[lane].data(),
                static_cast<std::uint16_t>(
                    ambilight::config::kTestedPhysicalLaneLength)
            };
        }
    }

    void fill(
        const Rgb8& color) {

        for (const auto& lane :
             frame.lane) {

            TEST_ASSERT_TRUE(
                lane.fill(
                    0,
                    lane.pixelCount,
                    color));
        }
    }

    Rgb8 pixel(
        std::size_t lane,
        std::uint16_t index) const {

        const auto& data =
            bytes[lane];

        const std::size_t base =
            static_cast<std::size_t>(
                index) *
            3U;

        return Rgb8{
            data[base + 1U],
            data[base],
            data[base + 2U]
        };
    }
};

} // namespace

void test_project_maps_segment_mask_to_runtime_lane() {
    LedMappingProfile topology;

    // Deliberately permute lanes to prove the physical mask follows the
    // runtime mapping rather than segment ordinal.
    topology.segment[
        static_cast<std::size_t>(
            SegmentId::Top)].
        lane = 3;

    topology.segment[
        static_cast<std::size_t>(
            SegmentId::Right)].
        lane = 2;

    topology.segment[
        static_cast<std::size_t>(
            SegmentId::Bottom)].
        lane = 1;

    topology.segment[
        static_cast<std::size_t>(
            SegmentId::Left)].
        lane = 0;

    TEST_ASSERT_TRUE(
        topology.valid());

    LedPixelMaskProfile profile;

    profile.disabledOffset[
        static_cast<std::size_t>(
            SegmentId::Top)] = 0;

    profile.disabledOffset[
        static_cast<std::size_t>(
            SegmentId::Left)] = 7;

    LedPhysicalPixelMask physical;

    TEST_ASSERT_TRUE(
        LedPhysicalPixelMask::project(
            topology,
            profile,
            physical));

    TEST_ASSERT_EQUAL_UINT16(
        7,
        physical.disabledOffset[0]);

    TEST_ASSERT_EQUAL_UINT16(
        LedPhysicalPixelMask::kNone,
        physical.disabledOffset[1]);

    TEST_ASSERT_EQUAL_UINT16(
        LedPhysicalPixelMask::kNone,
        physical.disabledOffset[2]);

    TEST_ASSERT_EQUAL_UINT16(
        0,
        physical.disabledOffset[3]);
}

void test_first_physical_pixel_stays_zero_on_reversed_side() {
    const LedMappingProfile topology;

    const auto top =
        topology.forSegment(
            SegmentId::Top);

    TEST_ASSERT_EQUAL_UINT8(
        1,
        top.reversed);

    LedPixelMaskProfile profile;

    profile.disabledOffset[
        static_cast<std::size_t>(
            SegmentId::Top)] = 0;

    LedPhysicalPixelMask physical;

    TEST_ASSERT_TRUE(
        LedPhysicalPixelMask::project(
            topology,
            profile,
            physical));

    // REV changes logical-to-physical rendering direction. It must never
    // reinterpret a persisted physical DATA-side offset.
    TEST_ASSERT_EQUAL_UINT16(
        0,
        physical.disabledOffset[
            top.lane]);

    Fixture fixture;

    fixture.fill(
        Rgb8{11, 22, 33});

    TEST_ASSERT_TRUE(
        physical.apply(
            fixture.frame));

    const Rgb8 first =
        fixture.pixel(
            top.lane,
            0);

    const Rgb8 last =
        fixture.pixel(
            top.lane,
            static_cast<std::uint16_t>(
                top.logicalLength -
                1U));

    TEST_ASSERT_EQUAL_UINT8(
        0,
        first.r);

    TEST_ASSERT_EQUAL_UINT8(
        0,
        first.g);

    TEST_ASSERT_EQUAL_UINT8(
        0,
        first.b);

    TEST_ASSERT_EQUAL_UINT8(
        11,
        last.r);

    TEST_ASSERT_EQUAL_UINT8(
        22,
        last.g);

    TEST_ASSERT_EQUAL_UINT8(
        33,
        last.b);
}

void test_apply_zeros_only_disabled_physical_pixels() {
    Fixture fixture;

    fixture.fill(
        Rgb8{10, 20, 30});

    LedPhysicalPixelMask mask;
    mask.disabledOffset[0] = 0;
    mask.disabledOffset[2] = 17;

    TEST_ASSERT_TRUE(
        mask.apply(
            fixture.frame));

    const Rgb8 lane0Masked =
        fixture.pixel(
            0,
            0);

    TEST_ASSERT_EQUAL_UINT8(
        0,
        lane0Masked.r);

    TEST_ASSERT_EQUAL_UINT8(
        0,
        lane0Masked.g);

    TEST_ASSERT_EQUAL_UINT8(
        0,
        lane0Masked.b);

    const Rgb8 lane0Adjacent =
        fixture.pixel(
            0,
            1);

    TEST_ASSERT_EQUAL_UINT8(
        10,
        lane0Adjacent.r);

    TEST_ASSERT_EQUAL_UINT8(
        20,
        lane0Adjacent.g);

    TEST_ASSERT_EQUAL_UINT8(
        30,
        lane0Adjacent.b);

    const Rgb8 lane2Masked =
        fixture.pixel(
            2,
            17);

    TEST_ASSERT_EQUAL_UINT8(
        0,
        lane2Masked.r);

    TEST_ASSERT_EQUAL_UINT8(
        0,
        lane2Masked.g);

    TEST_ASSERT_EQUAL_UINT8(
        0,
        lane2Masked.b);
}

void test_repeated_apply_keeps_disabled_pixel_black_after_raw_rewrite() {
    Fixture fixture;

    LedPhysicalPixelMask mask;
    mask.disabledOffset[1] = 0;

    fixture.frame.lane[1].
        writeUnchecked(
            0,
            Rgb8{255, 0, 0});

    TEST_ASSERT_TRUE(
        mask.apply(
            fixture.frame));

    TEST_ASSERT_EQUAL_UINT8(
        0,
        fixture.pixel(1, 0).r);

    // Simulate raw commissioning writing directly to the physical lane.
    fixture.frame.lane[1].
        writeUnchecked(
            0,
            Rgb8{0, 255, 0});

    TEST_ASSERT_TRUE(
        mask.apply(
            fixture.frame));

    const Rgb8 after =
        fixture.pixel(
            1,
            0);

    TEST_ASSERT_EQUAL_UINT8(
        0,
        after.r);

    TEST_ASSERT_EQUAL_UINT8(
        0,
        after.g);

    TEST_ASSERT_EQUAL_UINT8(
        0,
        after.b);
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(
        test_project_maps_segment_mask_to_runtime_lane);
    RUN_TEST(
        test_first_physical_pixel_stays_zero_on_reversed_side);

    RUN_TEST(
        test_apply_zeros_only_disabled_physical_pixels);

    RUN_TEST(
        test_repeated_apply_keeps_disabled_pixel_black_after_raw_rewrite);

    return UNITY_END();
}
