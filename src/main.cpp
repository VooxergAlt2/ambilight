#include <Arduino.h>
#include <WiFi.h>
#include <esp_err.h>
#include <esp_timer.h>
#include <lwip/inet.h>

#include "config/BoardConfig.h"
#include "core/FrameMailbox.h"
#include "core/RgbFrame.h"
#include "led/LedEngine.h"
#include "led/LedRenderer.h"
#include "network/DdpUdpService.h"
#include "network/WifiService.h"

namespace {

constexpr std::uint64_t kIdleBlackoutUs = 1000000;
constexpr std::uint32_t kStatusIntervalMs = 10000;

ambilight::LedEngine ledEngine;
ambilight::LedRenderer renderer(ledEngine);
ambilight::FrameMailbox mailbox;
ambilight::WifiService wifi;
ambilight::DdpUdpService ddp(mailbox);

ambilight::RgbFrame renderSnapshot;

std::uint32_t lastRenderedGeneration = 0;
std::uint32_t lastObservedDdpFrames = 0;
std::uint32_t idleBlackouts = 0;
std::uint32_t lastStatusMs = 0;

bool idleBlanked = false;

std::uint64_t lastFrameAgeUs = 0;
std::uint64_t maxFrameAgeUs = 0;

[[noreturn]] void fatal(const char* message, esp_err_t error) {
    Serial.printf("FATAL: %s: %s\n", message, esp_err_to_name(error));
    while (true) {
        delay(1000);
    }
}

void renderLatestFrame() {
    if (!mailbox.copyLatest(
            renderSnapshot,
            lastRenderedGeneration)) {
        return;
    }

    const std::uint64_t nowUs =
        static_cast<std::uint64_t>(esp_timer_get_time());

    lastFrameAgeUs =
        nowUs >= renderSnapshot.receivedUs
            ? nowUs - renderSnapshot.receivedUs
            : 0;

    if (lastFrameAgeUs > maxFrameAgeUs) {
        maxFrameAgeUs = lastFrameAgeUs;
    }

    const esp_err_t result = renderer.render(renderSnapshot);
    if (result != ESP_OK) {
        fatal("LedRenderer::render failed", result);
    }

    lastRenderedGeneration = renderSnapshot.generation;
}

void serviceIdleBlackout(std::uint64_t nowUs) {
    const std::uint64_t lastCompleteUs = ddp.lastCompleteFrameUs();

    if (lastCompleteUs == 0) {
        return;
    }

    if (nowUs - lastCompleteUs <= kIdleBlackoutUs) {
        return;
    }

    if (idleBlanked) {
        return;
    }

    ambilight::RgbFrame black;
    black.clear();
    black.receivedUs = nowUs;

    if (!mailbox.publish(black)) {
        fatal("idle blackout publish failed", ESP_FAIL);
    }

    idleBlanked = true;
    ++idleBlackouts;
}

void updateDdpActivityState() {
    const std::uint32_t completeFrames =
        ddp.stats().completeFramesPublished;

    if (completeFrames != lastObservedDdpFrames) {
        lastObservedDdpFrames = completeFrames;
        idleBlanked = false;
    }
}

void printRuntimeStatus() {
    char senderIp[INET_ADDRSTRLEN] = "none";

    const std::uint32_t sender =
        ddp.lastSenderIpv4NetworkOrder();

    if (sender != 0) {
        in_addr address{};
        address.s_addr = sender;
        inet_ntop(
            AF_INET,
            &address,
            senderIp,
            sizeof(senderIp));
    }

    const auto& udp = ddp.stats();
    const auto& asmStats = ddp.assemblerStats();

    Serial.printf(
        "STAT wifi=%s rssi=%d pkt=%lu frame=%lu rej=%lu stale=%lu timeout=%lu sup=%lu "
        "sockerr=%lu drainmax=%lu sender=%s:%u render=%lu maperr=%lu age=%lluus agemax=%lluus "
        "showmax=%luus black=%lu heap=%u minheap=%u\n",
        wifi.connected() ? "up" : "down",
        wifi.connected() ? WiFi.RSSI() : 0,
        static_cast<unsigned long>(udp.datagramsReceived),
        static_cast<unsigned long>(udp.completeFramesPublished),
        static_cast<unsigned long>(asmStats.rejected),
        static_cast<unsigned long>(asmStats.stale),
        static_cast<unsigned long>(asmStats.timedOut),
        static_cast<unsigned long>(asmStats.superseded),
        static_cast<unsigned long>(udp.socketErrors),
        static_cast<unsigned long>(udp.maxDatagramsPerPoll),
        senderIp,
        ddp.lastSenderPort(),
        static_cast<unsigned long>(renderer.renderedFrames()),
        static_cast<unsigned long>(renderer.mappingErrors()),
        static_cast<unsigned long long>(lastFrameAgeUs),
        static_cast<unsigned long long>(maxFrameAgeUs),
        static_cast<unsigned long>(ledEngine.maxShowTimeUs()),
        static_cast<unsigned long>(idleBlackouts),
        ESP.getFreeHeap(),
        ESP.getMinFreeHeap());
}

void printConfiguration() {
    Serial.println();
    Serial.println("ESP32-C6 Ambilight Stage 5: HyperHDR DDP runtime");
    Serial.printf(
        "Logical LEDs=%u RGB payload=%u bytes DDP port=%u\n",
        static_cast<unsigned>(ambilight::config::kLogicalLedCount),
        static_cast<unsigned>(
            ambilight::config::kLogicalLedCount *
            sizeof(ambilight::Rgb8)),
        ambilight::DdpUdpService::kPort);

    Serial.printf(
        "PARLIO lanes=%u lane_length=%u brightness=%u/255\n",
        static_cast<unsigned>(ambilight::config::kParlioLaneCount),
        static_cast<unsigned>(ambilight::config::kPhysicalLaneLength),
        static_cast<unsigned>(ambilight::config::kTestBrightness));

    Serial.println(
        "Runtime source: one HyperHDR DDP sender over Wi-Fi. "
        "USB/AWA and source arbitration are not present.");
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

    if (wifi.enabled()) {
        if (!ddp.begin()) {
            fatal("DdpUdpService::begin failed", ESP_FAIL);
        }

        Serial.printf(
            "DDP UDP socket bound to port %u. Waiting for HyperHDR.\n",
            ambilight::DdpUdpService::kPort);
    } else {
        Serial.println(
            "DDP runtime inactive because Wi-Fi credentials are absent.");
    }
}

void loop() {
    wifi.tick(millis());

    if (ddp.running()) {
        ddp.poll();
        updateDdpActivityState();
    }

    renderLatestFrame();

    const std::uint64_t nowUs =
        static_cast<std::uint64_t>(esp_timer_get_time());

    serviceIdleBlackout(nowUs);
    renderLatestFrame();

    const std::uint32_t nowMs = millis();
    if (static_cast<std::int32_t>(
            nowMs - (lastStatusMs + kStatusIntervalMs)) >= 0) {
        printRuntimeStatus();
        lastStatusMs = nowMs;
    }

    // C6 is single-core. Give lwIP/Wi-Fi system tasks a scheduling point
    // without adding a meaningful amount of Ambilight latency.
    delay(1);
}
