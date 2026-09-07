#include <unity.h>

#include "config/PanelConfig.h"
#include "led/LedMappingProfile.h"

using ambilight::LedMappingProfile;
using ambilight::SegmentId;

void test_measured_panel_side_gpio_and_direction() {
    const auto top =
        ambilight::config::panelLedSegment(
            SegmentId::Top);

    const auto right =
        ambilight::config::panelLedSegment(
            SegmentId::Right);

    const auto bottom =
        ambilight::config::panelLedSegment(
            SegmentId::Bottom);

    const auto left =
        ambilight::config::panelLedSegment(
            SegmentId::Left);

    TEST_ASSERT_EQUAL_UINT16(
        230,
        top.logicalLength);
    TEST_ASSERT_EQUAL_UINT8(
        20,
        top.gpio);
    TEST_ASSERT_TRUE(
        top.reversed);

    TEST_ASSERT_EQUAL_UINT16(
        160,
        right.logicalLength);
    TEST_ASSERT_EQUAL_UINT8(
        19,
        right.gpio);
    TEST_ASSERT_TRUE(
        right.reversed);

    TEST_ASSERT_EQUAL_UINT16(
        230,
        bottom.logicalLength);
    TEST_ASSERT_EQUAL_UINT8(
        21,
        bottom.gpio);
    TEST_ASSERT_TRUE(
        bottom.reversed);

    TEST_ASSERT_EQUAL_UINT16(
        160,
        left.logicalLength);
    TEST_ASSERT_EQUAL_UINT8(
        18,
        left.gpio);
    TEST_ASSERT_FALSE(
        left.reversed);
}

void test_measured_gpio_mapping_resolves_expected_parlio_lanes() {
    TEST_ASSERT_EQUAL_UINT8(
        2,
        ambilight::config::
            laneForLedGpio(20));

    TEST_ASSERT_EQUAL_UINT8(
        1,
        ambilight::config::
            laneForLedGpio(19));

    TEST_ASSERT_EQUAL_UINT8(
        3,
        ambilight::config::
            laneForLedGpio(21));

    TEST_ASSERT_EQUAL_UINT8(
        0,
        ambilight::config::
            laneForLedGpio(18));
}

void test_default_runtime_mapping_is_derived_from_hardware_truth() {
    const LedMappingProfile profile;

    const auto top =
        profile.forSegment(
            SegmentId::Top);

    const auto right =
        profile.forSegment(
            SegmentId::Right);

    const auto bottom =
        profile.forSegment(
            SegmentId::Bottom);

    const auto left =
        profile.forSegment(
            SegmentId::Left);

    TEST_ASSERT_EQUAL_UINT8(
        2,
        top.lane);
    TEST_ASSERT_EQUAL_UINT8(
        1,
        top.reversed);

    TEST_ASSERT_EQUAL_UINT8(
        1,
        right.lane);
    TEST_ASSERT_EQUAL_UINT8(
        1,
        right.reversed);

    TEST_ASSERT_EQUAL_UINT8(
        3,
        bottom.lane);
    TEST_ASSERT_EQUAL_UINT8(
        1,
        bottom.reversed);

    TEST_ASSERT_EQUAL_UINT8(
        0,
        left.lane);
    TEST_ASSERT_EQUAL_UINT8(
        0,
        left.reversed);

    TEST_ASSERT_EQUAL_UINT8(
        20,
        profile.gpioForSegment(
            SegmentId::Top));

    TEST_ASSERT_EQUAL_UINT8(
        19,
        profile.gpioForSegment(
            SegmentId::Right));

    TEST_ASSERT_EQUAL_UINT8(
        21,
        profile.gpioForSegment(
            SegmentId::Bottom));

    TEST_ASSERT_EQUAL_UINT8(
        18,
        profile.gpioForSegment(
            SegmentId::Left));
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(
        test_measured_panel_side_gpio_and_direction);

    RUN_TEST(
        test_measured_gpio_mapping_resolves_expected_parlio_lanes);

    RUN_TEST(
        test_default_runtime_mapping_is_derived_from_hardware_truth);

    return UNITY_END();
}
