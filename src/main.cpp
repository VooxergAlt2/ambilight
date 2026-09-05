#include <Arduino.h>
#include <esp_err.h>
#include <esp_timer.h>

#include "config/BoardConfig.h"
#include "core/FrameMailbox.h"
#include "core/Geometry.h"
#include "core/RgbFrame.h"
#include "led/LedEngine.h"
#include "led/LedRenderer.h"
#include "network/WifiService.h"

namespace {

using ambilight::Rgb8;
using ambilight::RgbFrame;
using ambilight::SegmentId;

constexpr Rgb8 kBlack   {0x00, 0x00, 0x00};
constexpr Rgb8 kRed     {0x20, 0x00, 0x00};
constexpr Rgb8 kGreen   {0x00, 0x20, 0x00};
constexpr Rgb8 kBlue    {0x00, 0x00, 0x20};
constexpr Rgb8 kYellow  {0x18, 0x18, 0x00};
constexpr Rgb8 kMagenta {0x18, 0x00, 0x18};
constexpr Rgb8 kCyan    {0x00, 0x18, 0x18};

ambilight::LedEngine ledEngine;
ambilight::LedRenderer renderer(ledEngine);
ambilight::FrameMailbox mailbox;
ambilight::WifiService wifi;

std::uint32_t lastRenderedGeneration = 0;

[[noreturn]] void fatal(const char* message, esp_err_t error) {
    Serial.printf("FATAL: %s: %s\n", message, esp_err_to_name(error));
    while (true) {
        delay(1000);
    }
}

void serviceBackground() {
    wifi.tick(millis());
}

void delayWithBackground(std::uint32_t durationMs) {
    const std::uint32_t started = millis();

    while (static_cast<std::uint32_t>(millis() - started) < durationMs) {
        serviceBackground();
        delay(10);
    }
}

const ambilight::SegmentConfig* findSegment(SegmentId id) {
    for (const auto& segment : ambilight::kSegments) {
        if (segment.id == id) {
            return &segment;
        }
    }
    return nullptr;
}

void fillSegment(RgbFrame& frame, SegmentId id, Rgb8 color) {
    const auto* segment = findSegment(id);
    if (segment == nullptr) {
        return;
    }

    const auto end = static_cast<std::uint16_t>(
        segment->logicalStart + segment->logicalLength);

    for (std::uint16_t logical = segment->logicalStart; logical < end; ++logical) {
        frame.pixels[logical] = color;
    }
}

void stampFrame(RgbFrame& frame) {
    frame.receivedUs = static_cast<std::uint64_t>(esp_timer_get_time());
}

void publishAndRender(RgbFrame& frame) {
    stampFrame(frame);

    if (!mailbox.publish(frame)) {
        fatal("FrameMailbox::publish failed", ESP_FAIL);
    }

    RgbFrame snapshot;
    if (!mailbox.copyLatest(snapshot, lastRenderedGeneration)) {
        fatal("FrameMailbox returned no newly published frame", ESP_FAIL);
    }

    const esp_err_t result = renderer.render(snapshot);
    if (result != ESP_OK) {
        fatal("LedRenderer::render failed", result);
    }

    lastRenderedGeneration = snapshot.generation;
}

void runIdentificationPattern() {
    RgbFrame frame;
    frame.clear();

    fillSegment(frame, SegmentId::Top, kRed);
    fillSegment(frame, SegmentId::Right, kGreen);
    fillSegment(frame, SegmentId::Bottom, kBlue);
    fillSegment(frame, SegmentId::Left, kYellow);

    publishAndRender(frame);
    delayWithBackground(2500);
}

void runBoundaryPattern() {
    RgbFrame frame;
    frame.clear();

    for (const auto& segment : ambilight::kSegments) {
        frame.pixels[segment.logicalStart] = kGreen;

        const auto middle = static_cast<std::uint16_t>(
            segment.logicalStart + segment.logicalLength / 2);
        frame.pixels[middle] = kBlue;

        const auto last = static_cast<std::uint16_t>(
            segment.logicalStart + segment.logicalLength - 1);
        frame.pixels[last] = kRed;
    }

    publishAndRender(frame);
    delayWithBackground(2500);
}

void runLogicalWalk() {
    RgbFrame frame;
    frame.clear();

    for (std::uint16_t logical = 0;
         logical < ambilight::config::kLogicalLedCount;
         ++logical) {

        if (logical > 0) {
            frame.pixels[logical - 1] = kBlack;
        }

        frame.pixels[logical] = kMagenta;
        publishAndRender(frame);
        serviceBackground();
        delay(6);
    }

    frame.clear();
    publishAndRender(frame);
}

void runFpsProbe(
    std::uint16_t targetFps,
    std::uint32_t durationMs) {

    if (targetFps == 0) {
        return;
    }

    RgbFrame frame;
    frame.clear();

    const std::uint32_t periodUs = 1000000UL / targetFps;
    const std::int64_t startedUs = esp_timer_get_time();
    const std::int64_t deadlineUs =
        startedUs + static_cast<std::int64_t>(durationMs) * 1000;

    std::uint32_t frames = 0;
    bool phase = false;

    while (esp_timer_get_time() < deadlineUs) {
        const std::int64_t frameStartedUs = esp_timer_get_time();

        fillSegment(frame, SegmentId::Top, phase ? kCyan : kBlue);
        fillSegment(frame, SegmentId::Right, phase ? kMagenta : kGreen);
        fillSegment(frame, SegmentId::Bottom, phase ? kYellow : kRed);
        fillSegment(frame, SegmentId::Left, phase ? kGreen : kCyan);
        phase = !phase;

        publishAndRender(frame);
        ++frames;

        serviceBackground();

        const std::int64_t elapsedUs = esp_timer_get_time() - frameStartedUs;
        if (elapsedUs < periodUs) {
            delayMicroseconds(
                static_cast<std::uint32_t>(periodUs - elapsedUs));
        } else {
            delay(0);
        }
    }

    const std::int64_t elapsedUs = esp_timer_get_time() - startedUs;
    const double actualFps =
        elapsedUs > 0
            ? static_cast<double>(frames) * 1000000.0 /
                  static_cast<double>(elapsedUs)
            : 0.0;

    Serial.printf(
        "Wi-Fi coexistence probe target=%u actual=%.2f frames=%lu rendered=%lu map_errors=%lu max_show=%luus\n",
        targetFps,
        actualFps,
        static_cast<unsigned long>(frames),
        static_cast<unsigned long>(renderer.renderedFrames()),
        static_cast<unsigned long>(renderer.mappingErrors()),
        static_cast<unsigned long>(ledEngine.maxShowTimeUs()));

    wifi.printStatus();

    frame.clear();
    publishAndRender(frame);
}

void printConfiguration() {
    Serial.println();
    Serial.println("ESP32-C6 Ambilight Stage 3: Wi-Fi coexistence + PARLIO");
    Serial.printf("Logical LEDs: %u, RGB payload: %u bytes\n",
                  static_cast<unsigned>(ambilight::config::kLogicalLedCount),
                  static_cast<unsigned>(
                      ambilight::config::kLogicalLedCount * sizeof(Rgb8)));
    Serial.printf("PARLIO lanes: %u, lane length: %u\n",
                  static_cast<unsigned>(ambilight::config::kParlioLaneCount),
                  static_cast<unsigned>(ambilight::config::kPhysicalLaneLength));
    Serial.printf("Stage brightness: %u/255\n",
                  static_cast<unsigned>(ambilight::config::kTestBrightness));

    for (const auto& segment : ambilight::kSegments) {
        Serial.printf(
            "segment=%u start=%u len=%u lane=%u gpio=%u reversed=%s\n",
            static_cast<unsigned>(segment.id),
            segment.logicalStart,
            segment.logicalLength,
            segment.lane,
            ambilight::config::kLedGpios[segment.lane],
            segment.reversed ? "yes" : "no");
    }

    Serial.println();
}

} // namespace

void setup() {
    Serial.begin(115200);
    delay(1000);

    printConfiguration();

    if (!mailbox.begin()) {
        fatal("FrameMailbox::begin failed", ESP_ERR_NO_MEM);
    }

    const esp_err_t ledResult = ledEngine.begin();
    if (ledResult != ESP_OK) {
        fatal("LedEngine::begin failed", ledResult);
    }

    if (!wifi.begin()) {
        fatal("WifiService::begin failed", ESP_FAIL);
    }

    Serial.printf("Initial PARLIO show completed in %lu us\n",
                  static_cast<unsigned long>(ledEngine.lastShowTimeUs()));
}

void loop() {
    serviceBackground();

    Serial.println("Pattern: Wi-Fi coexistence segment identification");
    runIdentificationPattern();

    Serial.println("Pattern: Wi-Fi coexistence segment boundaries");
    runBoundaryPattern();

    Serial.println("Pattern: Wi-Fi coexistence logical walk");
    runLogicalWalk();

    Serial.println("Probe: Wi-Fi enabled + renderer at 60 FPS for 10 seconds");
    runFpsProbe(60, 10000);

    Serial.println("Stage 3 cycle complete. Repeating in 3 seconds.\n");
    delayWithBackground(3000);
}
