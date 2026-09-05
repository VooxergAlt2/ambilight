#include <Arduino.h>
#include <esp_err.h>

#include "config/BoardConfig.h"
#include "led/LedEngine.h"

namespace {

ambilight::LedEngine ledEngine;

[[noreturn]] void fatal(const char* message, esp_err_t error) {
    Serial.printf("FATAL: %s: %s\n", message, esp_err_to_name(error));
    while (true) {
        delay(1000);
    }
}

void printConfiguration() {
    Serial.println();
    Serial.println("ESP32-C6 Ambilight Stage 1: PARLIO LED engine");
    Serial.printf("Logical LEDs: %u\n",
                  static_cast<unsigned>(ambilight::config::kLogicalLedCount));
    Serial.printf("PARLIO lanes: %u, lane length: %u\n",
                  static_cast<unsigned>(ambilight::config::kParlioLaneCount),
                  static_cast<unsigned>(ambilight::config::kPhysicalLaneLength));
    Serial.printf("Stage brightness: %u/255\n",
                  static_cast<unsigned>(ambilight::config::kTestBrightness));
    Serial.println("GPIO mapping is provisional until the exact C6 board is frozen.");

    for (const auto& segment : ambilight::LedEngine::segments()) {
        Serial.printf(
            "segment=%u start=%u len=%u lane=%u gpio=%u reversed=%s\n",
            static_cast<unsigned>(segment.id),
            segment.logicalStart,
            segment.logicalLength,
            segment.lane,
            segment.gpio,
            segment.reversed ? "yes" : "no");
    }
    Serial.println();
}

} // namespace

void setup() {
    Serial.begin(115200);
    delay(1000);

    printConfiguration();

    const esp_err_t result = ledEngine.begin();
    if (result != ESP_OK) {
        fatal("LedEngine::begin failed", result);
    }

    Serial.printf("Initial PARLIO show completed in %lu us\n",
                  static_cast<unsigned long>(ledEngine.lastShowTimeUs()));
}

void loop() {
    Serial.println("Pattern: segment identification");
    ledEngine.runIdentificationPattern(2500);

    Serial.println("Pattern: first/middle/last boundary markers");
    ledEngine.runBoundaryPattern(2500);

    Serial.println("Pattern: one logical pixel walking across all 780 LEDs");
    ledEngine.runLogicalWalk(6);

    Serial.println("Probe: 60 FPS for 5 seconds");
    ledEngine.runFpsProbe(60, 5000);

    Serial.printf("Maximum PARLIO show observed since boot: %lu us\n",
                  static_cast<unsigned long>(ledEngine.maxShowTimeUs()));
    Serial.println("Stage 1 cycle complete. Repeating in 3 seconds.\n");

    delay(3000);
}
