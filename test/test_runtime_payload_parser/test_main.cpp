#include <unity.h>

#include "runtime/RuntimePayloadParser.h"

using ambilight::LedMappingProfile;
using ambilight::LedPixelMaskProfile;
using ambilight::RuntimePayloadParseResult;
using ambilight::RuntimePayloadParser;
using ambilight::TofSpatialProfile;

void test_brightness_parses_valid_values() {
    std::uint8_t value = 99;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RuntimePayloadParseResult::Ok),
        static_cast<int>(
            RuntimePayloadParser::parseBrightness(
                "0",
                value)));

    TEST_ASSERT_EQUAL_UINT8(0, value);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RuntimePayloadParseResult::Ok),
        static_cast<int>(
            RuntimePayloadParser::parseBrightness(
                "255",
                value)));

    TEST_ASSERT_EQUAL_UINT8(255, value);
}

void test_brightness_distinguishes_empty_format_and_range() {
    std::uint8_t value = 42;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RuntimePayloadParseResult::Empty),
        static_cast<int>(
            RuntimePayloadParser::parseBrightness(
                "",
                value)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RuntimePayloadParseResult::InvalidFormat),
        static_cast<int>(
            RuntimePayloadParser::parseBrightness(
                "12x",
                value)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RuntimePayloadParseResult::OutOfRange),
        static_cast<int>(
            RuntimePayloadParser::parseBrightness(
                "256",
                value)));

    TEST_ASSERT_EQUAL_UINT8(
        42,
        value);
}

void test_led_mapping_parses_count_gpio_and_reversal() {
    LedMappingProfile profile;

    const auto result =
        RuntimePayloadParser::parseLedMapping(
            "200:21:1,150:20:0,210:19:1,140:18:0",
            profile);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            RuntimePayloadParseResult::Ok),
        static_cast<int>(result));

    TEST_ASSERT_EQUAL_UINT16(
        200,
        profile.segment[0].logicalLength);

    TEST_ASSERT_EQUAL_UINT8(
        3,
        profile.segment[0].lane);

    TEST_ASSERT_EQUAL_UINT8(
        1,
        profile.segment[0].reversed);

    TEST_ASSERT_EQUAL_UINT16(
        700,
        profile.totalLedCount());

    TEST_ASSERT_EQUAL_UINT8(
        18,
        profile.gpioForSegment(
            ambilight::SegmentId::Left));
}

void test_led_mapping_rejects_duplicate_gpio_or_bad_count() {
    LedMappingProfile profile;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            RuntimePayloadParseResult::OutOfRange),
        static_cast<int>(
            RuntimePayloadParser::parseLedMapping(
                "230:18:0,160:18:0,230:20:0,160:21:0",
                profile)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            RuntimePayloadParseResult::OutOfRange),
        static_cast<int>(
            RuntimePayloadParser::parseLedMapping(
                "231:18:0,160:19:0,230:20:0,160:21:0",
                profile)));
}

void test_led_mapping_rejects_bad_syntax() {
    LedMappingProfile profile;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            RuntimePayloadParseResult::InvalidFormat),
        static_cast<int>(
            RuntimePayloadParser::parseLedMapping(
                "230:18:0,160:19:0,230:20:0",
                profile)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            RuntimePayloadParseResult::InvalidFormat),
        static_cast<int>(
            RuntimePayloadParser::parseLedMapping(
                "230-18-0,160:19:0,230:20:0,160:21:0",
                profile)));
}

void test_commissioning_range_parses_side_and_gpio_targets() {
    ambilight::CommissioningRangeRequest request;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            RuntimePayloadParseResult::Ok),
        static_cast<int>(
            RuntimePayloadParser::
                parseCommissioningRange(
                    "side:2:100:10",
                    request)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            ambilight::
                CommissioningRangeTarget::
                    LogicalSide),
        static_cast<int>(
            request.target));

    TEST_ASSERT_EQUAL_UINT8(
        2,
        request.targetValue);

    TEST_ASSERT_EQUAL_UINT16(
        100,
        request.start);

    TEST_ASSERT_EQUAL_UINT16(
        10,
        request.count);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            RuntimePayloadParseResult::Ok),
        static_cast<int>(
            RuntimePayloadParser::
                parseCommissioningRange(
                    "gpio:20:0:37",
                    request)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            ambilight::
                CommissioningRangeTarget::
                    RawGpio),
        static_cast<int>(
            request.target));

    TEST_ASSERT_EQUAL_UINT8(
        20,
        request.targetValue);
}

void test_commissioning_range_rejects_unknown_gpio_and_overflow() {
    ambilight::CommissioningRangeRequest request;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            RuntimePayloadParseResult::OutOfRange),
        static_cast<int>(
            RuntimePayloadParser::
                parseCommissioningRange(
                    "gpio:22:0:10",
                    request)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            RuntimePayloadParseResult::OutOfRange),
        static_cast<int>(
            RuntimePayloadParser::
                parseCommissioningRange(
                    "side:0:220:11",
                    request)));
}

void test_led_pixel_mask_parses_none_and_segment_offsets() {
    LedPixelMaskProfile profile;

    const auto result =
        RuntimePayloadParser::parseLedPixelMask(
            "-,12,-,0",
            profile);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RuntimePayloadParseResult::Ok),
        static_cast<int>(result));

    TEST_ASSERT_EQUAL_UINT16(
        LedPixelMaskProfile::kNone,
        profile.disabledOffset[0]);

    TEST_ASSERT_EQUAL_UINT16(
        12,
        profile.disabledOffset[1]);

    TEST_ASSERT_EQUAL_UINT16(
        LedPixelMaskProfile::kNone,
        profile.disabledOffset[2]);

    TEST_ASSERT_EQUAL_UINT16(
        0,
        profile.disabledOffset[3]);

    TEST_ASSERT_TRUE(
        profile.disabled(
            ambilight::SegmentId::Right,
            12));

    TEST_ASSERT_FALSE(
        profile.disabled(
            ambilight::SegmentId::Right,
            13));
}

void test_led_pixel_mask_enforces_segment_lengths() {
    LedPixelMaskProfile profile;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RuntimePayloadParseResult::Ok),
        static_cast<int>(
            RuntimePayloadParser::parseLedPixelMask(
                "229,159,229,159",
                profile)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RuntimePayloadParseResult::OutOfRange),
        static_cast<int>(
            RuntimePayloadParser::parseLedPixelMask(
                "230,-,-,-",
                profile)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RuntimePayloadParseResult::Ok),
        static_cast<int>(
            RuntimePayloadParser::parseLedPixelMask(
                "-,160,-,-",
                profile)));

    TEST_ASSERT_FALSE(
        profile.validFor(
            LedMappingProfile{}));
}

void test_led_pixel_mask_rejects_bad_syntax_without_mutating_output() {
    LedPixelMaskProfile profile;
    profile.disabledOffset[0] = 7;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RuntimePayloadParseResult::InvalidFormat),
        static_cast<int>(
            RuntimePayloadParser::parseLedPixelMask(
                "-,12,-",
                profile)));

    TEST_ASSERT_EQUAL_UINT16(
        7,
        profile.disabledOffset[0]);
}

void test_spatial_profile_parses_fixed_point_and_negative_offsets() {
    TofSpatialProfile profile;

    const auto result =
        RuntimePayloadParser::parseSpatialProfile(
            "1437.5,1000,12.3,-45.6,7.8,3,1,10.5",
            profile);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RuntimePayloadParseResult::Ok),
        static_cast<int>(result));

    TEST_ASSERT_EQUAL_UINT16(
        14375,
        profile.widthMmX10);

    TEST_ASSERT_EQUAL_UINT16(
        10000,
        profile.heightMmX10);

    TEST_ASSERT_EQUAL_INT16(
        123,
        profile.sensorOffsetXmmX10);

    TEST_ASSERT_EQUAL_INT16(
        -456,
        profile.sensorOffsetYmmX10);

    TEST_ASSERT_EQUAL_INT16(
        78,
        profile.ledPlaneZmmX10);

    TEST_ASSERT_EQUAL_UINT8(
        3,
        profile.rotationQuarterTurns);

    TEST_ASSERT_EQUAL_UINT8(
        1,
        profile.mirrorX);

    TEST_ASSERT_EQUAL_UINT16(
        105,
        profile.planeDeadbandMmX10);
}

void test_spatial_profile_rejects_more_than_one_decimal_place() {
    TofSpatialProfile profile;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RuntimePayloadParseResult::InvalidFormat),
        static_cast<int>(
            RuntimePayloadParser::parseSpatialProfile(
                "1437.55,1000,0,0,0,0,0,10",
                profile)));
}

void test_spatial_profile_rejects_valid_syntax_outside_profile_bounds() {
    TofSpatialProfile profile;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RuntimePayloadParseResult::OutOfRange),
        static_cast<int>(
            RuntimePayloadParser::parseSpatialProfile(
                "50,1000,0,0,0,0,0,10",
                profile)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RuntimePayloadParseResult::OutOfRange),
        static_cast<int>(
            RuntimePayloadParser::parseSpatialProfile(
                "1437.5,1000,0,0,0,4,0,10",
                profile)));
}

void test_gain_curve_parses_valid_monotonic_points() {
    std::array<
        ambilight::GainPoint,
        ambilight::DistanceGainCurve::kMaxPoints>
        points{};

    std::size_t count = 0;

    const auto result =
        RuntimePayloadParser::parseGainCurve(
            "50:2048,500:3072,4000:4096",
            points,
            count);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RuntimePayloadParseResult::Ok),
        static_cast<int>(result));

    TEST_ASSERT_EQUAL_UINT32(
        3,
        count);

    TEST_ASSERT_EQUAL_UINT16(
        50,
        points[0].distanceMm);

    TEST_ASSERT_EQUAL_UINT16(
        2048,
        points[0].gainQ12);

    TEST_ASSERT_EQUAL_UINT16(
        4000,
        points[2].distanceMm);

    TEST_ASSERT_EQUAL_UINT16(
        4096,
        points[2].gainQ12);
}

void test_gain_curve_rejects_non_monotonic_distance_or_gain() {
    std::array<
        ambilight::GainPoint,
        ambilight::DistanceGainCurve::kMaxPoints>
        points{};

    std::size_t count = 99;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RuntimePayloadParseResult::OutOfRange),
        static_cast<int>(
            RuntimePayloadParser::parseGainCurve(
                "500:2048,400:4096",
                points,
                count)));

    TEST_ASSERT_EQUAL_UINT32(
        0,
        count);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RuntimePayloadParseResult::OutOfRange),
        static_cast<int>(
            RuntimePayloadParser::parseGainCurve(
                "500:4096,1000:2048",
                points,
                count)));
}

void test_gain_curve_rejects_gain_above_unity() {
    std::array<
        ambilight::GainPoint,
        ambilight::DistanceGainCurve::kMaxPoints>
        points{};

    std::size_t count = 0;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RuntimePayloadParseResult::OutOfRange),
        static_cast<int>(
            RuntimePayloadParser::parseGainCurve(
                "50:4096,500:4097",
                points,
                count)));
}

void test_gain_curve_rejects_malformed_payload() {
    std::array<
        ambilight::GainPoint,
        ambilight::DistanceGainCurve::kMaxPoints>
        points{};

    std::size_t count = 0;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RuntimePayloadParseResult::InvalidFormat),
        static_cast<int>(
            RuntimePayloadParser::parseGainCurve(
                "50-2048,500:3072",
                points,
                count)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RuntimePayloadParseResult::InvalidFormat),
        static_cast<int>(
            RuntimePayloadParser::parseGainCurve(
                "50:2048,",
                points,
                count)));
}

void test_gain_curve_rejects_more_than_eight_points() {
    std::array<
        ambilight::GainPoint,
        ambilight::DistanceGainCurve::kMaxPoints>
        points{};

    std::size_t count = 0;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RuntimePayloadParseResult::OutOfRange),
        static_cast<int>(
            RuntimePayloadParser::parseGainCurve(
                "1:1,2:2,3:3,4:4,5:5,6:6,7:7,8:8,9:9",
                points,
                count)));

    TEST_ASSERT_EQUAL_UINT32(
        0,
        count);
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_brightness_parses_valid_values);
    RUN_TEST(test_brightness_distinguishes_empty_format_and_range);
    RUN_TEST(test_led_mapping_parses_count_gpio_and_reversal);
    RUN_TEST(test_led_mapping_rejects_duplicate_gpio_or_bad_count);
    RUN_TEST(test_led_mapping_rejects_bad_syntax);
    RUN_TEST(test_commissioning_range_parses_side_and_gpio_targets);
    RUN_TEST(test_commissioning_range_rejects_unknown_gpio_and_overflow);
    RUN_TEST(test_led_pixel_mask_parses_none_and_segment_offsets);
    RUN_TEST(test_led_pixel_mask_enforces_segment_lengths);
    RUN_TEST(test_led_pixel_mask_rejects_bad_syntax_without_mutating_output);
    RUN_TEST(test_spatial_profile_parses_fixed_point_and_negative_offsets);
    RUN_TEST(test_spatial_profile_rejects_more_than_one_decimal_place);
    RUN_TEST(test_spatial_profile_rejects_valid_syntax_outside_profile_bounds);
    RUN_TEST(test_gain_curve_parses_valid_monotonic_points);
    RUN_TEST(test_gain_curve_rejects_non_monotonic_distance_or_gain);
    RUN_TEST(test_gain_curve_rejects_gain_above_unity);
    RUN_TEST(test_gain_curve_rejects_malformed_payload);
    RUN_TEST(test_gain_curve_rejects_more_than_eight_points);

    return UNITY_END();
}
