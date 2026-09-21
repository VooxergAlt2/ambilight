#include <cstddef>
#include <cstdint>

#include <unity.h>

#include "render/ManualLighting.h"

using ambilight::LedMappingProfile;
using ambilight::ManualLighting;
using ambilight::ManualLightingEffect;
using ambilight::ManualLightingState;
using ambilight::Rgb8;
using ambilight::RgbFrame;

void test_effect_ids_and_names_are_stable_for_wled() {
    TEST_ASSERT_EQUAL_UINT8(
        0,
        static_cast<std::uint8_t>(
            ManualLightingEffect::Ambilight));

    TEST_ASSERT_EQUAL_UINT8(
        1,
        static_cast<std::uint8_t>(
            ManualLightingEffect::Solid));

    TEST_ASSERT_EQUAL_UINT8(
        2,
        static_cast<std::uint8_t>(
            ManualLightingEffect::Rainbow));

    TEST_ASSERT_EQUAL_UINT8(
        3,
        static_cast<std::uint8_t>(
            ManualLightingEffect::Breathing));

    TEST_ASSERT_EQUAL_STRING(
        "Ambilight",
        ManualLighting::effectName(
            ManualLightingEffect::Ambilight));

    TEST_ASSERT_EQUAL_STRING(
        "Solid",
        ManualLighting::effectName(
            ManualLightingEffect::Solid));
}

void test_ambilight_is_not_rendered_by_manual_renderer() {
    ManualLightingState state;
    RgbFrame frame;

    TEST_ASSERT_FALSE(
        ManualLighting::render(
            state,
            LedMappingProfile{},
            1000000ULL,
            frame));
}

void test_solid_fills_entire_runtime_topology() {
    LedMappingProfile topology;

    topology.segment[0].logicalLength = 100;
    topology.segment[1].logicalLength = 80;
    topology.segment[2].logicalLength = 100;
    topology.segment[3].logicalLength = 80;

    TEST_ASSERT_TRUE(
        topology.valid());

    ManualLightingState state;
    state.effect =
        ManualLightingEffect::Solid;
    state.color =
        Rgb8{12, 34, 56};

    RgbFrame frame;

    TEST_ASSERT_TRUE(
        ManualLighting::render(
            state,
            topology,
            2000000ULL,
            frame));

    TEST_ASSERT_EQUAL_UINT16(
        360,
        frame.pixelCount);

    for (std::size_t index = 0;
         index < frame.pixelCount;
         ++index) {

        TEST_ASSERT_EQUAL_UINT8(
            12,
            frame.pixels[index].r);

        TEST_ASSERT_EQUAL_UINT8(
            34,
            frame.pixels[index].g);

        TEST_ASSERT_EQUAL_UINT8(
            56,
            frame.pixels[index].b);
    }
}

void test_rainbow_is_animated_and_spatially_non_uniform() {
    ManualLightingState state;
    state.effect =
        ManualLightingEffect::Rainbow;
    state.speed = 128;

    RgbFrame first;
    RgbFrame second;

    TEST_ASSERT_TRUE(
        ManualLighting::render(
            state,
            LedMappingProfile{},
            0,
            first));

    TEST_ASSERT_TRUE(
        ManualLighting::render(
            state,
            LedMappingProfile{},
            1000000ULL,
            second));

    TEST_ASSERT_TRUE(
        ManualLighting::animated(
            state.effect));

    const bool spatialDifference =
        first.pixels[0].r !=
            first.pixels[100].r ||
        first.pixels[0].g !=
            first.pixels[100].g ||
        first.pixels[0].b !=
            first.pixels[100].b;

    TEST_ASSERT_TRUE(
        spatialDifference);

    const bool temporalDifference =
        first.pixels[0].r !=
            second.pixels[0].r ||
        first.pixels[0].g !=
            second.pixels[0].g ||
        first.pixels[0].b !=
            second.pixels[0].b;

    TEST_ASSERT_TRUE(
        temporalDifference);
}

void test_breathing_uses_selected_color_and_intensity() {
    ManualLightingState state;
    state.effect =
        ManualLightingEffect::Breathing;
    state.color =
        Rgb8{200, 100, 50};
    state.speed = 255;
    state.intensity = 255;

    RgbFrame low;
    RgbFrame high;

    TEST_ASSERT_TRUE(
        ManualLighting::render(
            state,
            LedMappingProfile{},
            0,
            low));

    TEST_ASSERT_TRUE(
        ManualLighting::render(
            state,
            LedMappingProfile{},
            350000ULL,
            high));

    TEST_ASSERT_TRUE(
        high.pixels[0].r >
        low.pixels[0].r);

    TEST_ASSERT_TRUE(
        high.pixels[0].g >
        low.pixels[0].g);

    TEST_ASSERT_TRUE(
        high.pixels[0].b >
        low.pixels[0].b);
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(
        test_effect_ids_and_names_are_stable_for_wled);

    RUN_TEST(
        test_ambilight_is_not_rendered_by_manual_renderer);

    RUN_TEST(
        test_solid_fills_entire_runtime_topology);

    RUN_TEST(
        test_rainbow_is_animated_and_spatially_non_uniform);

    RUN_TEST(
        test_breathing_uses_selected_color_and_intensity);

    return UNITY_END();
}
