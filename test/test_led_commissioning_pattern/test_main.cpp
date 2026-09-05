#include <unity.h>

#include "led/LedCommissioningPattern.h"

using ambilight::LedCommissioningPattern;
using ambilight::LedCommissioningPatternBuilder;
using ambilight::Rgb8;
using ambilight::RgbFrame;

namespace {

void assertColor(
    const Rgb8& actual,
    std::uint8_t r,
    std::uint8_t g,
    std::uint8_t b) {

    TEST_ASSERT_EQUAL_UINT8(r, actual.r);
    TEST_ASSERT_EQUAL_UINT8(g, actual.g);
    TEST_ASSERT_EQUAL_UINT8(b, actual.b);
}

} // namespace

void test_none_pattern_is_black() {
    RgbFrame frame;

    LedCommissioningPatternBuilder::build(
        LedCommissioningPattern::None,
        frame);

    for (const auto& pixel :
         frame.pixels) {

        assertColor(
            pixel,
            0,
            0,
            0);
    }
}

void test_segment_identity_uses_four_logical_colors() {
    RgbFrame frame;

    LedCommissioningPatternBuilder::build(
        LedCommissioningPattern::SegmentIdentity,
        frame);

    assertColor(frame.pixels[0],   255, 0, 0);
    assertColor(frame.pixels[229], 255, 0, 0);

    assertColor(frame.pixels[230], 0, 255, 0);
    assertColor(frame.pixels[389], 0, 255, 0);

    assertColor(frame.pixels[390], 0, 0, 255);
    assertColor(frame.pixels[619], 0, 0, 255);

    assertColor(frame.pixels[620], 255, 255, 255);
    assertColor(frame.pixels[779], 255, 255, 255);
}

void test_direction_pattern_marks_start_middle_and_end() {
    RgbFrame frame;

    LedCommissioningPatternBuilder::build(
        LedCommissioningPattern::DirectionMarkers,
        frame);

    // TOP logical direction 0 -> 229.
    assertColor(frame.pixels[0],   255, 0, 0);
    assertColor(frame.pixels[4],   255, 0, 0);
    assertColor(frame.pixels[5],   8, 8, 8);

    assertColor(frame.pixels[113], 0, 255, 0);
    assertColor(frame.pixels[117], 0, 255, 0);

    assertColor(frame.pixels[224], 8, 8, 8);
    assertColor(frame.pixels[225], 0, 0, 255);
    assertColor(frame.pixels[229], 0, 0, 255);

    // RIGHT starts at logical 230 and has length 160.
    assertColor(frame.pixels[230], 255, 0, 0);
    assertColor(frame.pixels[310], 0, 255, 0);
    assertColor(frame.pixels[389], 0, 0, 255);
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_none_pattern_is_black);
    RUN_TEST(test_segment_identity_uses_four_logical_colors);
    RUN_TEST(test_direction_pattern_marks_start_middle_and_end);

    return UNITY_END();
}
