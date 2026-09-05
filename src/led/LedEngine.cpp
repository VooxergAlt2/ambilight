#include "led/LedEngine.h"

#include <Arduino.h>
#include <esp_timer.h>

namespace ambilight {
namespace {

constexpr crgb_t kBlack   = 0x000000;
constexpr crgb_t kRed     = 0x200000;
constexpr crgb_t kGreen   = 0x002000;
constexpr crgb_t kBlue    = 0x000020;
constexpr crgb_t kYellow  = 0x181800;
constexpr crgb_t kMagenta = 0x180018;
constexpr crgb_t kCyan    = 0x001818;

constexpr std::array<SegmentConfig, config::kParlioLaneCount> kSegments = {{
    {SegmentId::Top,    0,   230, 0, config::kLedGpios[0], false},
    {SegmentId::Right,  230, 160, 1, config::kLedGpios[1], false},
    {SegmentId::Bottom, 390, 230, 2, config::kLedGpios[2], false},
    {SegmentId::Left,   620, 160, 3, config::kLedGpios[3], false},
}};

static_assert(
    kSegments[3].logicalStart + kSegments[3].logicalLength ==
        config::kLogicalLedCount,
    "Segment map must cover exactly 780 logical LEDs");

constexpr std::array<crgb_t, config::kParlioLaneCount> kSegmentColors = {
    kRed, kGreen, kBlue, kYellow
};

} // namespace

LedEngine::LedEngine()
    : group_(LED_STRIP_WS2812, config::kPhysicalLaneLength, false) {}

const std::array<SegmentConfig, config::kParlioLaneCount>& LedEngine::segments() {
    return kSegments;
}

const SegmentConfig* LedEngine::findSegment(SegmentId id) {
    for (const auto& segment : kSegments) {
        if (segment.id == id) {
            return &segment;
        }
    }
    return nullptr;
}

esp_err_t LedEngine::begin() {
    for (std::size_t index = 0; index < kSegments.size(); ++index) {
        lanes_[index] = &group_.addStrip(kSegments[index].gpio);
    }

    const esp_err_t result = group_.begin();
    if (result != ESP_OK) {
        return result;
    }

    group_.brightness(config::kTestBrightness);

    clear();
    return show();
}

void LedEngine::clear() {
    for (auto* lane : lanes_) {
        if (lane != nullptr) {
            lane->clear();
        }
    }
}

void LedEngine::fillSegment(SegmentId id, crgb_t color) {
    const SegmentConfig* segment = findSegment(id);
    if (segment == nullptr || lanes_[segment->lane] == nullptr) {
        return;
    }

    auto* lane = lanes_[segment->lane];

    // Clear all 230 physical slots first. For 160-pixel segments this keeps
    // the 70 virtual tail pixels black.
    lane->clear();

    for (std::uint16_t pixel = 0; pixel < segment->logicalLength; ++pixel) {
        const std::uint16_t physical =
            segment->reversed
                ? static_cast<std::uint16_t>(segment->logicalLength - 1 - pixel)
                : pixel;
        lane->setPixel(physical, color);
    }
}

bool LedEngine::setLogicalPixel(std::uint16_t logicalIndex, crgb_t color) {
    if (logicalIndex >= config::kLogicalLedCount) {
        return false;
    }

    for (const auto& segment : kSegments) {
        const std::uint16_t end =
            static_cast<std::uint16_t>(segment.logicalStart + segment.logicalLength);

        if (logicalIndex < segment.logicalStart || logicalIndex >= end) {
            continue;
        }

        const std::uint16_t offset =
            static_cast<std::uint16_t>(logicalIndex - segment.logicalStart);
        const std::uint16_t physical =
            segment.reversed
                ? static_cast<std::uint16_t>(segment.logicalLength - 1 - offset)
                : offset;

        lanes_[segment.lane]->setPixel(physical, color);
        return true;
    }

    return false;
}

esp_err_t LedEngine::show() {
    const std::int64_t startedUs = esp_timer_get_time();
    const esp_err_t result = group_.show();
    const std::int64_t finishedUs = esp_timer_get_time();

    const auto elapsed = static_cast<std::uint32_t>(finishedUs - startedUs);
    lastShowTimeUs_ = elapsed;
    if (elapsed > maxShowTimeUs_) {
        maxShowTimeUs_ = elapsed;
    }

    return result;
}

void LedEngine::runIdentificationPattern(std::uint32_t holdMs) {
    clear();

    for (std::size_t index = 0; index < kSegments.size(); ++index) {
        fillSegment(kSegments[index].id, kSegmentColors[index]);
    }

    show();
    delay(holdMs);
}

void LedEngine::runBoundaryPattern(std::uint32_t holdMs) {
    clear();

    for (const auto& segment : kSegments) {
        setLogicalPixel(segment.logicalStart, kGreen);

        const auto midpoint = static_cast<std::uint16_t>(
            segment.logicalStart + segment.logicalLength / 2);
        setLogicalPixel(midpoint, kBlue);

        const auto last = static_cast<std::uint16_t>(
            segment.logicalStart + segment.logicalLength - 1);
        setLogicalPixel(last, kRed);
    }

    show();
    delay(holdMs);
}

void LedEngine::runLogicalWalk(std::uint32_t stepMs) {
    clear();

    for (std::uint16_t logical = 0; logical < config::kLogicalLedCount; ++logical) {
        if (logical > 0) {
            setLogicalPixel(
                static_cast<std::uint16_t>(logical - 1),
                kBlack);
        }

        setLogicalPixel(logical, kMagenta);
        show();
        delay(stepMs);
    }

    clear();
    show();
}

void LedEngine::runFpsProbe(std::uint16_t targetFps, std::uint32_t durationMs) {
    if (targetFps == 0) {
        return;
    }

    const std::uint32_t periodUs = 1000000UL / targetFps;
    const std::int64_t probeStartedUs = esp_timer_get_time();
    const std::int64_t probeDeadlineUs =
        probeStartedUs + static_cast<std::int64_t>(durationMs) * 1000;

    std::uint32_t renderedFrames = 0;
    std::uint64_t totalShowUs = 0;
    std::uint32_t localMaxShowUs = 0;
    bool phase = false;

    while (esp_timer_get_time() < probeDeadlineUs) {
        const std::int64_t frameStartedUs = esp_timer_get_time();

        fillSegment(SegmentId::Top, phase ? kCyan : kBlue);
        fillSegment(SegmentId::Right, phase ? kMagenta : kGreen);
        fillSegment(SegmentId::Bottom, phase ? kYellow : kRed);
        fillSegment(SegmentId::Left, phase ? kGreen : kCyan);
        phase = !phase;

        const esp_err_t result = show();
        if (result != ESP_OK) {
            Serial.printf("PARLIO show failed during FPS probe: %s\n",
                          esp_err_to_name(result));
            break;
        }

        ++renderedFrames;
        totalShowUs += lastShowTimeUs_;
        if (lastShowTimeUs_ > localMaxShowUs) {
            localMaxShowUs = lastShowTimeUs_;
        }

        const std::int64_t elapsedUs = esp_timer_get_time() - frameStartedUs;
        if (elapsedUs < periodUs) {
            delayMicroseconds(
                static_cast<std::uint32_t>(periodUs - elapsedUs));
        } else {
            // Give system tasks a scheduling point when the requested rate is
            // above the actual LED engine capacity.
            delay(0);
        }
    }

    const std::int64_t actualElapsedUs = esp_timer_get_time() - probeStartedUs;
    const double actualFps =
        actualElapsedUs > 0
            ? (static_cast<double>(renderedFrames) * 1000000.0) /
                  static_cast<double>(actualElapsedUs)
            : 0.0;
    const double averageShowUs =
        renderedFrames > 0
            ? static_cast<double>(totalShowUs) /
                  static_cast<double>(renderedFrames)
            : 0.0;

    Serial.printf(
        "FPS probe target=%u actual=%.2f frames=%lu avg_show=%.1fus max_show=%luus\n",
        targetFps,
        actualFps,
        static_cast<unsigned long>(renderedFrames),
        averageShowUs,
        static_cast<unsigned long>(localMaxShowUs));

    clear();
    show();
}

} // namespace ambilight
