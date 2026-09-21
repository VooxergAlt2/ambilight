#include <cstddef>
#include <cstdint>

#include <unity.h>

#include "render/BlackFrameForensics.h"

using ambilight::BlackFrameForensics;
using ambilight::BlackFrameReason;
using ambilight::LedMappingProfile;
using ambilight::LedPixelMaskProfile;
using ambilight::RenderGainContext;
using ambilight::Rgb8;
using ambilight::RgbFrame;
using ambilight::SegmentId;
using ambilight::kGainUnityQ12;

namespace {

RgbFrame makeFrame(
    const LedMappingProfile& topology,
    Rgb8 color) {

    RgbFrame frame;
    frame.pixelCount =
        topology.totalLedCount();

    for (std::size_t index = 0;
         index < frame.pixelCount;
         ++index) {

        frame.pixels[index] =
            color;
    }

    return frame;
}

RenderGainContext makeGain(
    const LedMappingProfile& topology,
    std::uint16_t gainQ12) {

    RenderGainContext context;
    context.topology = topology;
    context.sourcePresent = true;
    context.sourceUsable = true;
    context.failOpen = false;

    for (std::size_t index = 0;
         index <
            topology.totalLedCount();
         ++index) {

        context.logicalGainQ12[index] =
            gainQ12;
    }

    return context;
}

} // namespace

void test_source_black_is_classified_and_retained() {
    const LedMappingProfile topology;
    const LedPixelMaskProfile mask;

    BlackFrameForensics tracker;

    auto frame =
        makeFrame(
            topology,
            Rgb8{});

    frame.generation = 11;

    TEST_ASSERT_TRUE(
        tracker.observe(
            frame,
            topology,
            mask,
            253,
            nullptr,
            1000000));

    const auto& stats =
        tracker.stats();

    TEST_ASSERT_EQUAL_UINT32(
        1,
        stats.blackEvents);

    TEST_ASSERT_EQUAL_UINT32(
        1,
        stats.sourceBlackFrames);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            BlackFrameReason::SourceBlack),
        static_cast<int>(
            stats.lastBlack.reason));

    TEST_ASSERT_EQUAL_UINT32(
        11,
        stats.lastBlack.generation);

    TEST_ASSERT_EQUAL_UINT8(
        0,
        stats.lastBlack.sourceMaxChannel);
}

void test_active_gain_black_is_distinguished_from_source_black() {
    const LedMappingProfile topology;
    const LedPixelMaskProfile mask;

    BlackFrameForensics tracker;

    auto frame =
        makeFrame(
            topology,
            Rgb8{120, 60, 30});

    frame.generation = 22;

    const auto gain =
        makeGain(
            topology,
            0);

    TEST_ASSERT_TRUE(
        tracker.observe(
            frame,
            topology,
            mask,
            253,
            &gain,
            2000000));

    const auto& sample =
        tracker.stats().lastBlack;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            BlackFrameReason::ActiveGain),
        static_cast<int>(
            sample.reason));

    TEST_ASSERT_EQUAL_UINT8(
        120,
        sample.sourceMaxChannel);

    TEST_ASSERT_EQUAL_UINT8(
        0,
        sample.outputMaxChannel);

    TEST_ASSERT_EQUAL_UINT16(
        topology.totalLedCount(),
        sample.zeroGainPixels);

    TEST_ASSERT_EQUAL_UINT16(
        0,
        sample.gainMinQ12);

    TEST_ASSERT_EQUAL_UINT16(
        0,
        sample.gainMaxQ12);
}

void test_brightness_zero_is_an_explicit_black_reason() {
    const LedMappingProfile topology;
    const LedPixelMaskProfile mask;

    BlackFrameForensics tracker;

    const auto frame =
        makeFrame(
            topology,
            Rgb8{255, 1, 1});

    TEST_ASSERT_TRUE(
        tracker.observe(
            frame,
            topology,
            mask,
            0,
            nullptr,
            3000000));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            BlackFrameReason::BrightnessZero),
        static_cast<int>(
            tracker.stats().lastBlack.reason));
}

void test_pixel_mask_reason_is_detected_when_it_removes_only_lit_pixel() {
    const LedMappingProfile topology;
    LedPixelMaskProfile mask;

    RgbFrame frame =
        makeFrame(
            topology,
            Rgb8{});

    const auto top =
        topology.segmentConfig(
            SegmentId::Top);

    // TOP is reversed in the measured default topology. Physical LED 0
    // therefore corresponds to the last logical offset of the segment.
    frame.pixels[
        top.logicalStart +
        top.logicalLength -
        1U] =
        Rgb8{10, 20, 30};

    mask.disabledOffset[
        static_cast<std::size_t>(
            SegmentId::Top)] = 0;

    BlackFrameForensics tracker;

    TEST_ASSERT_TRUE(
        tracker.observe(
            frame,
            topology,
            mask,
            255,
            nullptr,
            4000000));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            BlackFrameReason::PixelMask),
        static_cast<int>(
            tracker.stats().lastBlack.reason));
}

void test_black_events_count_transitions_not_every_black_frame() {
    const LedMappingProfile topology;
    const LedPixelMaskProfile mask;

    BlackFrameForensics tracker;

    auto lit =
        makeFrame(
            topology,
            Rgb8{1, 0, 0});

    auto black =
        makeFrame(
            topology,
            Rgb8{});

    TEST_ASSERT_TRUE(
        tracker.observe(
            lit,
            topology,
            mask,
            255,
            nullptr,
            1000));

    TEST_ASSERT_TRUE(
        tracker.observe(
            black,
            topology,
            mask,
            255,
            nullptr,
            2000));

    TEST_ASSERT_TRUE(
        tracker.observe(
            black,
            topology,
            mask,
            255,
            nullptr,
            3000));

    TEST_ASSERT_TRUE(
        tracker.observe(
            lit,
            topology,
            mask,
            255,
            nullptr,
            4000));

    TEST_ASSERT_TRUE(
        tracker.observe(
            black,
            topology,
            mask,
            255,
            nullptr,
            5000));

    const auto& stats =
        tracker.stats();

    TEST_ASSERT_EQUAL_UINT32(
        3,
        stats.blackFrames);

    TEST_ASSERT_EQUAL_UINT32(
        2,
        stats.blackEvents);

    TEST_ASSERT_EQUAL_UINT32(
        2,
        stats.maxConsecutiveBlackFrames);
}

void test_normal_active_frame_reports_gain_range_without_black() {
    const LedMappingProfile topology;
    const LedPixelMaskProfile mask;

    BlackFrameForensics tracker;

    const auto frame =
        makeFrame(
            topology,
            Rgb8{80, 40, 20});

    const auto gain =
        makeGain(
            topology,
            2048);

    TEST_ASSERT_TRUE(
        tracker.observe(
            frame,
            topology,
            mask,
            253,
            &gain,
            6000000));

    const auto& latest =
        tracker.stats().latest;

    TEST_ASSERT_FALSE(
        latest.outputBlack);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            BlackFrameReason::None),
        static_cast<int>(
            latest.reason));

    TEST_ASSERT_EQUAL_UINT16(
        2048,
        latest.gainMinQ12);

    TEST_ASSERT_EQUAL_UINT16(
        2048,
        latest.gainMaxQ12);

    TEST_ASSERT_EQUAL_UINT16(
        0,
        latest.zeroGainPixels);

    TEST_ASSERT_TRUE(
        latest.outputMaxChannel > 0);
}

void test_invalid_frame_is_rejected_without_false_black_event() {
    const LedMappingProfile topology;
    const LedPixelMaskProfile mask;

    BlackFrameForensics tracker;
    RgbFrame frame;
    frame.pixelCount = 1;

    TEST_ASSERT_FALSE(
        tracker.observe(
            frame,
            topology,
            mask,
            255,
            nullptr,
            7000000));

    TEST_ASSERT_EQUAL_UINT32(
        1,
        tracker.stats().invalidSamples);

    TEST_ASSERT_EQUAL_UINT32(
        0,
        tracker.stats().framesObserved);

    TEST_ASSERT_EQUAL_UINT32(
        0,
        tracker.stats().blackEvents);
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(
        test_source_black_is_classified_and_retained);

    RUN_TEST(
        test_active_gain_black_is_distinguished_from_source_black);

    RUN_TEST(
        test_brightness_zero_is_an_explicit_black_reason);

    RUN_TEST(
        test_pixel_mask_reason_is_detected_when_it_removes_only_lit_pixel);

    RUN_TEST(
        test_black_events_count_transitions_not_every_black_frame);

    RUN_TEST(
        test_normal_active_frame_reports_gain_range_without_black);

    RUN_TEST(
        test_invalid_frame_is_rejected_without_false_black_event);

    return UNITY_END();
}
