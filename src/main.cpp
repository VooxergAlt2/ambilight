#include <Arduino.h>
#include <WiFi.h>

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>

#include "config/BoardConfig.h"
#include "config/FirmwareInfo.h"
#include "config/RuntimeSettings.h"
#include "config/TofCalibration.h"
#include "config/WifiCredentials.h"
#include "core/FrameMailbox.h"
#include "core/LatencyHistogram.h"
#include "core/RgbFrame.h"
#include "led/LedCommissioningPattern.h"
#include "led/LedEngine.h"
#include "led/LedRenderer.h"
#include "integration/TofRenderGainBridge.h"
#include "render/CorrectionMode.h"
#include "render/RenderGainController.h"
#include "render/ShadowGainProbe.h"
#include "render/RenderScheduler.h"
#include "runtime/RuntimePayloadParser.h"
#include "runtime/SerialCommandParser.h"
#include "network/DdpUdpService.h"
#include "network/WebUiService.h"
#include "network/WifiService.h"
#include "tof/TofCalibrationCapture.h"
#include "tof/TofDebugGrid.h"
#include "tof/TofService.h"

// Stage 39 LED-reset diagnostics: give the Arduino loop task enough headroom
// to distinguish a real driver/power reset from commissioning stack pressure.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

namespace {

constexpr std::uint64_t kIdleBlackoutUs = 1000000;
constexpr std::uint32_t kStatusIntervalMs = 30000;
constexpr std::uint8_t kMaxConsecutiveBacklogRenderSkips = 4;
constexpr std::uint64_t kGainTargetPollIntervalUs = 1000000;
constexpr std::uint64_t kCommissioningDurationUs = 15000000ULL;
constexpr std::uint8_t kCommissioningMaxBrightness = 64;
constexpr std::uint64_t kTofDebugDurationUs = 60000000ULL;

ambilight::LedEngine ledEngine;
ambilight::LedRenderer renderer(ledEngine);
ambilight::FrameMailbox mailbox;
ambilight::WifiService wifi;
ambilight::DdpUdpService ddp(mailbox);
ambilight::LatencyHistogram frameAgeHistogram;
ambilight::TofService tof;
ambilight::TofCalibrationCapture calibrationCapture;
ambilight::RenderGainController renderGainController;
ambilight::RenderScheduler renderScheduler;
ambilight::RuntimeSettings runtimeSettings;
ambilight::WebUiService webUi;

ambilight::CorrectionMode correctionMode =
    ambilight::CorrectionMode::Shadow;

ambilight::RgbFrame renderSnapshot;
ambilight::PerimeterGainSnapshot cachedPerimeterGainSnapshot{};
ambilight::RenderGainContext cachedTargetGainContext{};
// Persistent unity context for commissioning. Keeping this global avoids a
// ~1.9 KiB temporary RenderGainContext on the loop-task stack at test start.
ambilight::RenderGainContext commissioningGainContext{};

bool rgbFrameValid = false;
bool rgbDirty = false;
bool correctionModeDirty = false;
bool brightnessDirty = false;
bool ledMappingDirty = false;
bool ledPixelMaskDirty = false;

ambilight::SerialCommandParser serialCommandParser;

std::uint32_t lastWebActionSequence = 0;
bool lastWebActionOk = true;
std::array<char, 96> lastWebActionMessage{};

bool haveLastCalibrationSummary = false;
ambilight::CalibrationCaptureSummary lastCalibrationSummary{};

enum class WifiCredentialSource : std::uint8_t {
    None = 0,
    CompileTime,
    Nvs,
    RuntimeVolatile
};

WifiCredentialSource wifiCredentialSource =
    WifiCredentialSource::None;

ambilight::LedCommissioningPattern commissioningPattern =
    ambilight::LedCommissioningPattern::None;
ambilight::RgbFrame commissioningFrame;
std::uint64_t commissioningUntilUs = 0;

ambilight::SegmentId commissioningRangeSegment =
    ambilight::SegmentId::Top;

std::uint8_t commissioningRawLane = 0;
std::uint16_t commissioningRangeStart = 0;
std::uint16_t commissioningRangeCount = 0;

bool commissioningDirty = false;
std::uint32_t commissioningRuns = 0;
std::uint32_t commissioningRenders = 0;
std::uint32_t commissioningCancels = 0;

bool haveCachedPerimeterGainSnapshot = false;

std::uint32_t lastMailboxGeneration = 0;
std::uint32_t lastRenderedRgbGeneration = 0;
std::uint32_t lastObservedPublications = 0;
std::uint32_t idleBlackouts = 0;
std::uint32_t lastStatusMs = 0;
std::uint32_t backlogRenderSkips = 0;
std::uint32_t calibrationLastGeometryGeneration = 0;
std::uint32_t shadowProbeGeneration = 0;
std::uint64_t shadowProbeUntilUs = 0;
std::uint64_t tofDebugUntilUs = 0;
std::uint64_t nextGainTargetPollUs = 0;

std::uint8_t consecutiveBacklogRenderSkips = 0;

bool idleBlanked = false;

std::uint64_t lastFrameAgeUs = 0;
std::uint64_t maxFrameAgeUs = 0;

bool shadowGainProbeActive();
bool fillWebUiSnapshot(
    ambilight::WebUiSnapshot& snapshot);
void handleWebUiAction(
    ambilight::WebUiActionEvent event);

const char* resetReasonName(
    esp_reset_reason_t reason) {

    switch (reason) {
    case ESP_RST_UNKNOWN:
        return "UNKNOWN";
    case ESP_RST_POWERON:
        return "POWERON";
    case ESP_RST_EXT:
        return "EXT";
    case ESP_RST_SW:
        return "SW";
    case ESP_RST_PANIC:
        return "PANIC";
    case ESP_RST_INT_WDT:
        return "INT_WDT";
    case ESP_RST_TASK_WDT:
        return "TASK_WDT";
    case ESP_RST_WDT:
        return "WDT";
    case ESP_RST_DEEPSLEEP:
        return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:
        return "BROWNOUT";
    case ESP_RST_SDIO:
        return "SDIO";
    default:
        return "OTHER";
    }
}

void printLedRuntimeProbe(
    const char* tag) {

    const UBaseType_t stackHighWater =
        uxTaskGetStackHighWaterMark(nullptr);

    const std::size_t dmaFree =
        heap_caps_get_free_size(
            MALLOC_CAP_DMA |
            MALLOC_CAP_INTERNAL);

    const std::size_t dmaLargest =
        heap_caps_get_largest_free_block(
            MALLOC_CAP_DMA |
            MALLOC_CAP_INTERNAL);

    Serial.printf(
        "LED DIAG tag=%s stack_hwm=%lu stack_unit=%uB heap=%lu minheap=%lu dma_free=%lu dma_largest=%lu\n",
        tag != nullptr ? tag : "none",
        static_cast<unsigned long>(
            stackHighWater),
        static_cast<unsigned>(
            sizeof(StackType_t)),
        static_cast<unsigned long>(
            ESP.getFreeHeap()),
        static_cast<unsigned long>(
            ESP.getMinFreeHeap()),
        static_cast<unsigned long>(
            dmaFree),
        static_cast<unsigned long>(
            dmaLargest));
}

const char* wifiCredentialSourceName(
    WifiCredentialSource source) {

    switch (source) {
    case WifiCredentialSource::None:
        return "NONE";
    case WifiCredentialSource::CompileTime:
        return "COMPILE_TIME";
    case WifiCredentialSource::Nvs:
        return "NVS";
    case WifiCredentialSource::RuntimeVolatile:
        return "RUNTIME_VOLATILE";
    }

    return "INVALID";
}

bool ensureDdpRunning() {
    if (!wifi.enabled()) {
        return false;
    }

    if (ddp.running()) {
        return true;
    }

    if (!ddp.begin()) {
        Serial.println(
            "DDP start failed after Wi-Fi configuration.");
        return false;
    }

    const auto& socketStats =
        ddp.stats();

    Serial.printf(
        "DDP socket bound to UDP/%u. RXBUF requested=%ld actual=%ld set=%s query=%s option_warnings=%lu.\n",
        ambilight::DdpUdpService::kPort,
        static_cast<long>(socketStats.requestedRxBufferBytes),
        static_cast<long>(socketStats.actualRxBufferBytes),
        socketStats.rxBufferSetOk ? "ok" : "no",
        socketStats.rxBufferQueryOk ? "ok" : "no",
        static_cast<unsigned long>(
            socketStats.socketOptionWarnings));

    return true;
}

bool ensureWebUiRunning() {
    if (!wifi.enabled()) {
        return false;
    }

    if (webUi.running()) {
        return true;
    }

    if (!webUi.begin()) {
        Serial.println(
            "Web UI warning: HTTP/80 listener could not start. Core Ambilight continues.");
        return false;
    }

    Serial.printf(
        "Web UI listening on HTTP/%u. No external assets or web framework loaded.\n",
        static_cast<unsigned>(
            ambilight::WebUiService::kPort));

    return true;
}

void printWifiProvisioningStatus() {
    wifi.printStatus();

    Serial.printf(
        "Wi-Fi credentials source=%s persisted_nvs=%s password=hidden\n",
        wifiCredentialSourceName(
            wifiCredentialSource),
        wifiCredentialSource ==
                WifiCredentialSource::Nvs
            ? "yes"
            : "no");
}

bool applyWifiCredentials(
    const char* ssid,
    const char* password,
    bool persist) {

    bool persisted = false;

    if (persist) {
        persisted =
            runtimeSettings.setWifiCredentials(
                ssid,
                password);
    }

    if (!wifi.configure(
            ssid,
            password)) {

        Serial.println(
            "Wi-Fi credentials rejected: SSID/password length is invalid.");
        return false;
    }

    wifiCredentialSource =
        persisted
            ? WifiCredentialSource::Nvs
            : WifiCredentialSource::RuntimeVolatile;

    if (!ensureDdpRunning()) {
        Serial.println(
            "Wi-Fi configured, but DDP socket is not running.");
    }

    ensureWebUiRunning();

    Serial.printf(
        "Wi-Fi credentials applied: ssid='%s' source=%s password=hidden\n",
        wifi.ssid(),
        wifiCredentialSourceName(
            wifiCredentialSource));

    return true;
}

bool clearRuntimeWifiCredentials() {
    const bool cleared =
        runtimeSettings.clearWifiCredentials();

    if (!cleared &&
        runtimeSettings.persistenceAvailable()) {

        Serial.println(
            "Wi-Fi clear failed: persisted credentials remain authoritative; live networking unchanged.");
        return false;
    }

    if (ambilight::config::wifiCredentialsPresent()) {
        if (!wifi.configure(
                ambilight::config::kWifiSsid,
                ambilight::config::kWifiPassword)) {

            webUi.stop();
            wifi.disable();
            ddp.stop();

            wifiCredentialSource =
                WifiCredentialSource::None;

            Serial.println(
                "Compile-time Wi-Fi fallback is invalid; Wi-Fi/DDP disabled.");
            return cleared;
        }

        wifiCredentialSource =
            WifiCredentialSource::CompileTime;

        ensureDdpRunning();
        ensureWebUiRunning();

        Serial.printf(
            "Wi-Fi NVS credentials cleared (%s); using compile-time fallback SSID '%s'.\n",
            cleared ? "persisted" : "volatile-only",
            wifi.ssid());

        return cleared;
    }

    webUi.stop();
    wifi.disable();
    ddp.stop();

    wifiCredentialSource =
        WifiCredentialSource::None;

    Serial.printf(
        "Wi-Fi NVS credentials cleared (%s); no compile-time fallback, Wi-Fi/DDP disabled.\n",
        cleared ? "persisted" : "volatile-only");

    return cleared;
}

void handleWifiCommand(
    char* command) {

    if (command == nullptr ||
        command[0] == '\0') {

        printWifiProvisioningStatus();
        return;
    }

    if (std::strcmp(
            command,
            "clear") == 0) {

        clearRuntimeWifiCredentials();
        return;
    }

    char* separator =
        std::strchr(
            command,
            '|');

    if (separator == nullptr) {
        Serial.println(
            "Wi-Fi command invalid. Use wSSID|PASSWORD or wclear.");
        return;
    }

    *separator = '\0';

    const char* ssid =
        command;

    const char* password =
        separator + 1;

    applyWifiCredentials(
        ssid,
        password,
        true);
}

bool handleFactoryCommand(
    const char* command) {

    if (command == nullptr ||
        std::strcmp(
            command,
            "reset") != 0) {

        Serial.println(
            "FACTORY RESET not executed. Use freset + Enter with brightness=0.");
        return false;
    }

    if (ledEngine.brightness() != 0) {
        Serial.printf(
            "FACTORY RESET refused: brightness must be 0. Current=%u.\n",
            static_cast<unsigned>(
                ledEngine.brightness()));

        return false;
    }

    // Best-effort physical blackout before clearing persistent configuration.
    ledEngine.clear();

    const esp_err_t blackoutResult =
        ledEngine.show();

    if (blackoutResult != ESP_OK) {
        Serial.printf(
            "FACTORY RESET warning: pre-reset LED blackout failed: %s\n",
            esp_err_to_name(
                blackoutResult));
    }

    if (!runtimeSettings.factoryReset()) {
        Serial.println(
            "FACTORY RESET failed: NVS namespace was not cleared. Runtime continues unchanged.");
        return false;
    }

    Serial.println(
        "FACTORY RESET complete. Restarting with firmware defaults.");
    Serial.flush();

    delay(100);
    ESP.restart();

    // ESP.restart() should not return, but keep the function contract explicit
    // for host/static analysis.
    return true;
}

const char* ledMappingSourceName() {
    if (!runtimeSettings.ledMappingProfileCustomized()) {
        return "DEFAULT";
    }

    return runtimeSettings.ledMappingProfilePersisted()
        ? "CUSTOM_NVS"
        : "CUSTOM_RUNTIME";
}

void printLedMappingProfile() {
    const auto& profile =
        runtimeSettings.ledMappingProfile();

    static constexpr const char* kNames[] = {
        "TOP", "RIGHT", "BOTTOM", "LEFT"
    };

    Serial.printf(
        "LED TOPOLOGY source=%s total=%u ddp_bytes=%u",
        ledMappingSourceName(),
        static_cast<unsigned>(
            profile.totalLedCount()),
        static_cast<unsigned>(
            profile.totalLedCount() *
            sizeof(ambilight::Rgb8)));

    for (std::size_t index = 0;
         index < profile.segment.size();
         ++index) {

        const auto& mapping =
            profile.segment[index];

        Serial.printf(
            " %s=count%u:GPIO%u:%s",
            kNames[index],
            static_cast<unsigned>(
                mapping.logicalLength),
            static_cast<unsigned>(
                ambilight::config::kLedGpios[
                    mapping.lane]),
            mapping.reversed
                ? "REV"
                : "FWD");
    }

    Serial.println();
}

void refreshRgbCache();

void invalidateRuntimeAfterTopologyChange(
    const ambilight::LedMappingProfile& profile) {

    cachedPerimeterGainSnapshot = {};
    haveCachedPerimeterGainSnapshot = false;

    cachedTargetGainContext =
        ambilight::RenderGainContext::unity(
            profile);

    renderGainController.reset();
    correctionModeDirty = true;
    ledMappingDirty = true;
    nextGainTargetPollUs = 0;

    ambilight::RgbFrame black;
    black.clear();
    black.pixelCount =
        profile.totalLedCount();

    black.receivedUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    if (!mailbox.publish(
            black)) {

        Serial.println(
            "LED TOPOLOGY warning: could not publish topology blackout frame.");
    } else {
        refreshRgbCache();
    }
}

bool activateLedMappingProfile(
    const ambilight::LedMappingProfile& profile,
    ambilight::LedPixelMaskProfile& effectiveMask,
    bool& maskChanged) {

    maskChanged = false;

    if (ledEngine.brightness() != 0) {
        Serial.println(
            "LED TOPOLOGY change refused: set output brightness to 0 first.");
        return false;
    }

    if (!profile.valid()) {
        Serial.println(
            "LED TOPOLOGY profile is invalid.");
        return false;
    }

    const ambilight::LedMappingProfile previous =
        runtimeSettings.
            ledMappingProfile();

    // The ToF task is the only normal runtime participant that can refuse an
    // otherwise valid topology because its mutex is busy. Queue it first.
    if (!tof.setLedTopology(
            profile)) {

        Serial.println(
            "LED TOPOLOGY change refused: ToF service could not queue topology.");
        return false;
    }

    // With a valid profile, DDP and renderer validation should be infallible.
    // Keep rollback anyway so a future contract change cannot leave mixed
    // subsystem topology.
    if (!ddp.setLogicalLedCount(
            profile.totalLedCount())) {

        tof.setLedTopology(
            previous);

        Serial.println(
            "LED TOPOLOGY change refused: DDP frame size rejected; ToF rollback requested.");
        return false;
    }

    if (!renderer.setMappingProfile(
            profile)) {

        ddp.setLogicalLedCount(
            previous.totalLedCount());

        tof.setLedTopology(
            previous);

        Serial.println(
            "LED TOPOLOGY change refused: renderer rejected profile; DDP/ToF rollback requested.");
        return false;
    }

    // LedRenderer clears every physical PARLIO lane buffer whenever mapping
    // actually changes. That makes capacity outside a shortened/remapped side
    // deterministically black before brightness can be restored.

    effectiveMask =
        runtimeSettings.
            ledPixelMaskProfile();

    const auto originalMask =
        effectiveMask.disabledOffset;

    effectiveMask.sanitizeFor(
        profile);

    maskChanged =
        effectiveMask.disabledOffset !=
        originalMask;

    if (!renderer.setPixelMaskProfile(
            effectiveMask)) {

        // This should be unreachable after sanitizeFor(profile), but preserve
        // the old live topology if the renderer contract changes later.
        renderer.setMappingProfile(
            previous);

        ddp.setLogicalLedCount(
            previous.totalLedCount());

        tof.setLedTopology(
            previous);

        renderer.setPixelMaskProfile(
            runtimeSettings.
                ledPixelMaskProfile());

        Serial.println(
            "LED TOPOLOGY change refused: effective pixel mask was rejected; rollback requested.");
        return false;
    }

    return true;
}

void commitSanitizedMaskAfterTopology(
    const ambilight::LedPixelMaskProfile& mask,
    bool maskChanged) {

    if (!maskChanged) {
        return;
    }

    const bool maskPersisted =
        runtimeSettings.
            setLedPixelMaskProfile(
                mask);

    ledPixelMaskDirty = true;

    Serial.printf(
        "LED TOPOLOGY shortened a side; out-of-range disabled-pixel entries were cleared (%s).\n",
        maskPersisted
            ? "persisted"
            : "runtime-only");
}

bool applyLedMappingProfile(
    const ambilight::LedMappingProfile& profile) {

    ambilight::LedPixelMaskProfile effectiveMask;
    bool maskChanged = false;

    if (!activateLedMappingProfile(
            profile,
            effectiveMask,
            maskChanged)) {

        return false;
    }

    const bool persisted =
        runtimeSettings.setLedMappingProfile(
            profile);

    commitSanitizedMaskAfterTopology(
        effectiveMask,
        maskChanged);

    invalidateRuntimeAfterTopologyChange(
        profile);

    Serial.printf(
        "LED TOPOLOGY applied; total=%u ddp_bytes=%u persisted=%s. DDP sender lease reset; HyperHDR must use the same LED count.\n",
        static_cast<unsigned>(
            profile.totalLedCount()),
        static_cast<unsigned>(
            profile.totalLedCount() *
            sizeof(ambilight::Rgb8)),
        persisted ? "yes" : "no");

    printLedMappingProfile();
    return true;
}

bool resetLedMappingProfile() {
    const ambilight::LedMappingProfile previous =
        runtimeSettings.
            ledMappingProfile();

    const ambilight::LedMappingProfile profile;

    ambilight::LedPixelMaskProfile effectiveMask;
    bool maskChanged = false;

    if (!activateLedMappingProfile(
            profile,
            effectiveMask,
            maskChanged)) {

        return false;
    }

    const bool persisted =
        runtimeSettings.resetLedMappingProfile();

    if (!persisted &&
        runtimeSettings.persistenceAvailable()) {

        ambilight::LedPixelMaskProfile rollbackMask;
        bool rollbackMaskChanged = false;

        // Durable reset was refused before its version commit marker could be
        // removed. Restore the live subsystems and original mask to the still
        // authoritative previous runtime profile.
        if (!activateLedMappingProfile(
                previous,
                rollbackMask,
                rollbackMaskChanged)) {

            Serial.println(
                "LED TOPOLOGY reset failed and live rollback could not be completed; keep brightness=0 and reboot.");
        } else {
            renderer.setPixelMaskProfile(
                runtimeSettings.
                    ledPixelMaskProfile());

            invalidateRuntimeAfterTopologyChange(
                previous);
        }

        Serial.println(
            "LED TOPOLOGY reset failed: persisted custom topology remains authoritative.");
        return false;
    }

    commitSanitizedMaskAfterTopology(
        effectiveMask,
        maskChanged);

    invalidateRuntimeAfterTopologyChange(
        profile);

    Serial.printf(
        "LED TOPOLOGY reset to default; persisted=%s.\n",
        persisted ? "yes" : "runtime-only");

    printLedMappingProfile();
    return true;
}

void handleLedMapCommand(
    const char* command) {

    if (command == nullptr ||
        command[0] == '\0') {

        printLedMappingProfile();
        return;
    }

    if (std::strcmp(
            command,
            "reset") == 0) {

        resetLedMappingProfile();
        return;
    }

    ambilight::LedMappingProfile profile;

    const auto parseResult =
        ambilight::RuntimePayloadParser::
            parseLedMapping(
                command,
                profile);

    if (parseResult !=
        ambilight::RuntimePayloadParseResult::Ok) {

        Serial.println(
            "LED TOPOLOGY invalid. Use lCOUNT:GPIO:REV,... Example: l230:18:0,160:19:0,230:20:0,160:21:0");
        return;
    }

    applyLedMappingProfile(
        profile);
}

const char* ledPixelMaskSourceName() {
    if (!runtimeSettings.ledPixelMaskProfileCustomized()) {
        return "DEFAULT";
    }

    return
        runtimeSettings.ledPixelMaskProfilePersisted()
            ? "CUSTOM_NVS"
            : "CUSTOM_RUNTIME";
}

void printLedPixelMaskProfile() {
    const auto& profile =
        runtimeSettings.ledPixelMaskProfile();

    static constexpr const char* kNames[] = {
        "TOP", "RIGHT", "BOTTOM", "LEFT"
    };

    Serial.printf(
        "LED PIXEL MASK source=%s",
        ledPixelMaskSourceName());

    for (std::size_t index = 0;
         index < profile.disabledOffset.size();
         ++index) {

        const std::uint16_t value =
            profile.disabledOffset[index];

        if (value ==
            ambilight::LedPixelMaskProfile::kNone) {

            Serial.printf(
                " %s=none",
                kNames[index]);
        } else {
            Serial.printf(
                " %s=%u",
                kNames[index],
                static_cast<unsigned>(
                    value));
        }
    }

    Serial.println();
}

bool applyLedPixelMaskProfile(
    const ambilight::LedPixelMaskProfile& profile) {

    if (!profile.validFor(
            runtimeSettings.
                ledMappingProfile())) {

        Serial.println(
            "LED PIXEL MASK profile is invalid for the active side lengths.");
        return false;
    }

    if (!renderer.setPixelMaskProfile(
            profile)) {

        Serial.println(
            "LED PIXEL MASK could not be applied.");
        return false;
    }

    const bool persisted =
        runtimeSettings.setLedPixelMaskProfile(
            profile);

    ledPixelMaskDirty = true;

    Serial.printf(
        "LED PIXEL MASK applied; persisted=%s.\n",
        persisted ? "yes" : "no");

    printLedPixelMaskProfile();
    return true;
}

bool resetLedPixelMaskProfile() {
    const bool persisted =
        runtimeSettings.resetLedPixelMaskProfile();

    if (!persisted &&
        runtimeSettings.persistenceAvailable()) {

        Serial.println(
            "LED PIXEL MASK reset failed: persisted mask remains active.");
        return false;
    }

    if (!renderer.setPixelMaskProfile(
            runtimeSettings.ledPixelMaskProfile())) {

        Serial.println(
            "LED PIXEL MASK default profile is invalid.");
        return false;
    }

    ledPixelMaskDirty = true;

    Serial.printf(
        "LED PIXEL MASK reset; persisted=%s.\n",
        persisted ? "yes" : "runtime-only");

    printLedPixelMaskProfile();
    return true;
}

void handleLedPixelMaskCommand(
    const char* command) {

    if (command == nullptr ||
        command[0] == '\0') {

        printLedPixelMaskProfile();
        return;
    }

    if (std::strcmp(
            command,
            "reset") == 0) {

        resetLedPixelMaskProfile();
        return;
    }

    ambilight::LedPixelMaskProfile profile;

    const auto parseResult =
        ambilight::RuntimePayloadParser::
            parseLedPixelMask(
                command,
                profile);

    if (parseResult !=
        ambilight::RuntimePayloadParseResult::Ok) {

        Serial.println(
            "LED PIXEL MASK invalid. Use dTOP,RIGHT,BOTTOM,LEFT with '-' for none. Example: d-,12,-,0");
        return;
    }

    applyLedPixelMaskProfile(
        profile);
}

const char* spatialProfileSourceName() {
    if (!runtimeSettings.tofSpatialProfileCustomized()) {
        return "DEFAULT";
    }

    return
        runtimeSettings.tofSpatialProfilePersisted()
            ? "CUSTOM_NVS"
            : "CUSTOM_RUNTIME";
}

void printSpatialProfile() {
    const auto& profile =
        runtimeSettings.tofSpatialProfile();

    Serial.printf(
        "TOF SPATIAL PROFILE source=%s width=%.1fmm height=%.1fmm "
        "sensor_x=%.1fmm sensor_y=%.1fmm led_z=%.1fmm rot=%u mirror=%u deadband=%.1fmm\n",
        spatialProfileSourceName(),
        static_cast<double>(
            profile.widthMm()),
        static_cast<double>(
            profile.heightMm()),
        static_cast<double>(
            profile.sensorOffsetXmm()),
        static_cast<double>(
            profile.sensorOffsetYmm()),
        static_cast<double>(
            profile.ledPlaneZmm()),
        static_cast<unsigned>(
            profile.rotationQuarterTurns),
        static_cast<unsigned>(
            profile.mirrorX),
        static_cast<double>(
            profile.planeDeadbandMm()));
}

void invalidateRenderedGainAfterSpatialChange() {
    cachedPerimeterGainSnapshot = {};
    haveCachedPerimeterGainSnapshot = false;

    cachedTargetGainContext =
        ambilight::RenderGainContext::unity(runtimeSettings.ledMappingProfile());

    renderGainController.reset();

    correctionModeDirty = true;
    nextGainTargetPollUs = 0;
}

bool applySpatialProfile(
    const ambilight::TofSpatialProfile& profile) {

    if (correctionMode ==
        ambilight::CorrectionMode::Active) {

        Serial.println(
            "TOF SPATIAL change refused in ACTIVE mode. Switch to SHADOW or DISABLED first.");
        return false;
    }

    if (!profile.valid()) {
        Serial.println(
            "TOF SPATIAL profile is invalid.");
        return false;
    }

    if (!tof.setSpatialProfile(
            profile)) {

        Serial.println(
            "TOF SPATIAL profile could not be queued to sensor service.");
        return false;
    }

    const bool persisted =
        runtimeSettings.setTofSpatialProfile(
            profile);

    invalidateRenderedGainAfterSpatialChange();

    Serial.printf(
        "TOF SPATIAL profile applied; persisted=%s. Waiting for next valid pose rebuild.\n",
        persisted ? "yes" : "no");

    printSpatialProfile();
    return true;
}

bool resetSpatialProfile() {
    if (correctionMode ==
        ambilight::CorrectionMode::Active) {

        Serial.println(
            "TOF SPATIAL reset refused in ACTIVE mode. Switch to SHADOW or DISABLED first.");
        return false;
    }

    const ambilight::TofSpatialProfile previous =
        runtimeSettings.tofSpatialProfile();

    const ambilight::TofSpatialProfile profile;

    if (!tof.setSpatialProfile(
            profile)) {

        Serial.println(
            "TOF SPATIAL reset refused: sensor service could not queue the default profile.");
        return false;
    }

    const bool persisted =
        runtimeSettings.resetTofSpatialProfile();

    if (!persisted &&
        runtimeSettings.persistenceAvailable()) {

        if (!tof.setSpatialProfile(
                previous)) {

            Serial.println(
                "TOF SPATIAL reset failed and live rollback could not be queued; keep correction out of ACTIVE and reboot.");
        }

        Serial.println(
            "TOF SPATIAL reset failed: persisted profile remains authoritative.");
        return false;
    }

    invalidateRenderedGainAfterSpatialChange();

    Serial.printf(
        "TOF SPATIAL profile reset to default; persisted=%s. Waiting for next valid pose rebuild.\n",
        persisted ? "yes" : "runtime-only");

    printSpatialProfile();
    return true;
}

void handleSpatialCommand(
    const char* command) {

    if (command == nullptr ||
        command[0] == '\0') {

        printSpatialProfile();
        return;
    }

    if (std::strcmp(
            command,
            "reset") == 0) {

        resetSpatialProfile();
        return;
    }

    ambilight::TofSpatialProfile profile;

    const auto parseResult =
        ambilight::RuntimePayloadParser::
            parseSpatialProfile(
                command,
                profile);

    if (parseResult !=
        ambilight::RuntimePayloadParseResult::Ok) {

        Serial.println(
            "TOF SPATIAL command invalid. Example: y1437.5,1000,0,0,0,0,0,10");
        return;
    }

    applySpatialProfile(
        profile);
}

const char* gainCurveSourceName() {
    if (!runtimeSettings.tofGainCurveCustomized()) {
        return "DEFAULT";
    }

    return
        runtimeSettings.tofGainCurvePersisted()
            ? "CUSTOM_NVS"
            : "CUSTOM_RUNTIME";
}

void printTofGainCurve() {
    const auto& points =
        runtimeSettings.tofGainPoints();

    const std::size_t count =
        runtimeSettings.tofGainPointCount();

    Serial.printf(
        "TOF CURVE source=%s points=%u",
        gainCurveSourceName(),
        static_cast<unsigned>(count));

    for (std::size_t index = 0;
         index < count;
         ++index) {

        const std::uint32_t percentX10 =
            (
                static_cast<std::uint32_t>(
                    points[index].gainQ12) *
                1000U +
                ambilight::kGainUnityQ12 / 2U
            ) /
            ambilight::kGainUnityQ12;

        Serial.printf(
            " [%u:%u=%lu.%lu%%]",
            points[index].distanceMm,
            points[index].gainQ12,
            static_cast<unsigned long>(
                percentX10 / 10U),
            static_cast<unsigned long>(
                percentX10 % 10U));
    }

    Serial.println();
}

void invalidateRenderedGainAfterCurveChange() {
    cachedTargetGainContext =
        ambilight::RenderGainContext::unity(runtimeSettings.ledMappingProfile());

    renderGainController.reset();

    correctionModeDirty = true;
    nextGainTargetPollUs = 0;
}

bool applyTofGainCurve(
    const std::array<
        ambilight::GainPoint,
        ambilight::DistanceGainCurve::kMaxPoints>& points,
    std::size_t count) {

    if (correctionMode ==
        ambilight::CorrectionMode::Active) {

        Serial.println(
            "TOF CURVE change refused in ACTIVE mode. Switch to SHADOW or DISABLED first.");
        return false;
    }

    const ambilight::DistanceGainCurve curve(
        points,
        count);

    if (!curve.valid()) {
        Serial.println(
            "TOF CURVE invalid: require 2..8 increasing distances, non-decreasing gains, gain <= 4096.");
        return false;
    }

    if (!tof.setGainCurve(
            curve)) {

        Serial.println(
            "TOF CURVE could not be queued to sensor service.");
        return false;
    }

    const bool persisted =
        runtimeSettings.setTofGainCurve(
            points,
            count);

    invalidateRenderedGainAfterCurveChange();

    Serial.printf(
        "TOF CURVE applied; points=%u persisted=%s. Waiting for next valid ToF pose rebuild.\n",
        static_cast<unsigned>(count),
        persisted ? "yes" : "no");

    printTofGainCurve();
    return true;
}

bool resetTofGainCurve() {
    if (correctionMode ==
        ambilight::CorrectionMode::Active) {

        Serial.println(
            "TOF CURVE reset refused in ACTIVE mode. Switch to SHADOW or DISABLED first.");
        return false;
    }

    const ambilight::DistanceGainCurve previous =
        runtimeSettings.tofGainCurve();

    const ambilight::DistanceGainCurve curve(
        ambilight::config::kTofGainPoints,
        ambilight::config::kTofGainPointCount);

    if (!tof.setGainCurve(
            curve)) {

        Serial.println(
            "TOF CURVE reset refused: sensor service could not queue the default curve.");
        return false;
    }

    const bool persisted =
        runtimeSettings.resetTofGainCurve();

    if (!persisted &&
        runtimeSettings.persistenceAvailable()) {

        if (!tof.setGainCurve(
                previous)) {

            Serial.println(
                "TOF CURVE reset failed and live rollback could not be queued; keep correction out of ACTIVE and reboot.");
        }

        Serial.println(
            "TOF CURVE reset failed: persisted curve remains authoritative.");
        return false;
    }

    invalidateRenderedGainAfterCurveChange();

    Serial.printf(
        "TOF CURVE reset to default; persisted=%s. Waiting for next valid ToF pose rebuild.\n",
        persisted ? "yes" : "runtime-only");

    printTofGainCurve();
    return true;
}

void handleGainCurveCommand(
    const char* command) {

    if (command == nullptr ||
        command[0] == '\0') {

        printTofGainCurve();
        return;
    }

    if (std::strcmp(
            command,
            "reset") == 0) {

        resetTofGainCurve();
        return;
    }

    std::array<
        ambilight::GainPoint,
        ambilight::DistanceGainCurve::kMaxPoints>
        points{};

    std::size_t count = 0;

    const auto parseResult =
        ambilight::RuntimePayloadParser::
            parseGainCurve(
                command,
                points,
                count);

    if (parseResult !=
        ambilight::RuntimePayloadParseResult::Ok) {

        Serial.println(
            "TOF CURVE command invalid. Example: q50:2048,500:3072,4000:4096");
        return;
    }

    applyTofGainCurve(
        points,
        count);
}

void printFirmwareInfo() {
    Serial.printf(
        "FW name=%s version=%s stage=%u target=%s serial_proto=%u "
        "logical_leds=%u logical_capacity=%u ddp_bytes=%u ddp_port=%u spatial_schema=%u ledmap_schema=%u pixelmask_schema=%u\n",
        ambilight::config::kFirmwareName,
        ambilight::config::kFirmwareVersion,
        static_cast<unsigned>(
            ambilight::config::kDevelopmentStage),
        ambilight::config::kFirmwareTarget,
        static_cast<unsigned>(
            ambilight::config::kSerialProtocolVersion),
        static_cast<unsigned>(
            runtimeSettings.
                ledMappingProfile().
                totalLedCount()),
        static_cast<unsigned>(
            ambilight::config::
                kLogicalLedCapacity),
        static_cast<unsigned>(
            runtimeSettings.
                ledMappingProfile().
                totalLedCount() *
            sizeof(ambilight::Rgb8)),
        static_cast<unsigned>(
            ambilight::DdpUdpService::kPort),
        static_cast<unsigned>(
            ambilight::TofSpatialProfile::kSchemaVersion),
        static_cast<unsigned>(
            ambilight::LedMappingProfile::kSchemaVersion),
        static_cast<unsigned>(
            ambilight::LedPixelMaskProfile::kSchemaVersion));
}

void printCorrectionMode() {
    Serial.printf(
        "CORRECTION mode=%s persisted=%s nvs=%s writes=%lu write_fail=%lu invalid_stored=%lu\n",
        ambilight::correctionModeName(
            correctionMode),
        runtimeSettings.persistenceAvailable()
            ? "yes"
            : "no",
        runtimeSettings.persistenceAvailable()
            ? "available"
            : "unavailable",
        static_cast<unsigned long>(
            runtimeSettings.stats().writes),
        static_cast<unsigned long>(
            runtimeSettings.stats().writeFailures),
        static_cast<unsigned long>(
            runtimeSettings.stats().invalidStoredValues));
}

void printOutputBrightness() {
    const std::uint8_t brightness =
        ledEngine.brightness();

    const std::uint32_t percentX10 =
        (
            static_cast<std::uint32_t>(
                brightness) *
            1000U +
            127U
        ) /
        255U;

    Serial.printf(
        "OUTPUT brightness=%u/255 (%lu.%lu%%) persisted=%s\n",
        static_cast<unsigned>(brightness),
        static_cast<unsigned long>(
            percentX10 / 10U),
        static_cast<unsigned long>(
            percentX10 % 10U),
        runtimeSettings.persistenceAvailable()
            ? "yes"
            : "no");
}

void setOutputBrightness(
    std::uint8_t brightness) {

    const bool persisted =
        runtimeSettings.setOutputBrightness(
            brightness);

    ledEngine.setBrightness(
        brightness);

    brightnessDirty = true;

    Serial.printf(
        "OUTPUT brightness changed to %u/255; persisted=%s.\n",
        static_cast<unsigned>(brightness),
        persisted ? "yes" : "no");
}

void setCorrectionMode(
    ambilight::CorrectionMode mode) {

    if (correctionMode == mode) {
        printCorrectionMode();
        return;
    }

    correctionMode = mode;

    const bool persisted =
        runtimeSettings.setCorrectionMode(
            mode);

    // Every mode transition starts the valid correction path from unity.
    // ACTIVE therefore fades in rather than suddenly applying a previously
    // accumulated shadow profile.
    renderGainController.reset();

    // A debug-only synthetic profile must never survive a mode transition.
    shadowProbeUntilUs = 0;

    correctionModeDirty = true;
    nextGainTargetPollUs = 0;

    Serial.printf(
        "CORRECTION changed to %s; persisted=%s.\n",
        ambilight::correctionModeName(mode),
        persisted ? "yes" : "no");
}

[[noreturn]] void fatal(const char* message, esp_err_t error) {
    Serial.printf("FATAL: %s: %s\n", message, esp_err_to_name(error));
    while (true) {
        delay(1000);
    }
}

const char* tofStateName(ambilight::TofState state) {
    switch (state) {
    case ambilight::TofState::NotStarted:
        return "not-started";
    case ambilight::TofState::Initializing:
        return "initializing";
    case ambilight::TofState::Ranging:
        return "ranging";
    case ambilight::TofState::Error:
        return "error";
    }

    return "unknown";
}

void refreshRgbCache() {
    if (!mailbox.copyLatest(
            renderSnapshot,
            lastMailboxGeneration)) {
        return;
    }

    lastMailboxGeneration =
        renderSnapshot.generation;

    rgbFrameValid = true;
    rgbDirty = true;
}

const char* commissioningPatternName(
    ambilight::LedCommissioningPattern pattern) {

    switch (pattern) {
    case ambilight::LedCommissioningPattern::None:
        return "NONE";
    case ambilight::LedCommissioningPattern::SegmentIdentity:
        return "SEGMENTS";
    case ambilight::LedCommissioningPattern::DirectionMarkers:
        return "DIRECTION";
    case ambilight::LedCommissioningPattern::LogicalRange:
        return "LOGICAL_RANGE";
    case ambilight::LedCommissioningPattern::RawPhysicalRange:
        return "RAW_GPIO_RANGE";
    }

    return "INVALID";
}

void printCommissioningStatus() {
    const std::uint64_t nowUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    const std::uint64_t remainingMs =
        commissioningPattern !=
                ambilight::LedCommissioningPattern::None &&
            commissioningUntilUs > nowUs
            ? (commissioningUntilUs - nowUs) /
                1000ULL
            : 0ULL;

    Serial.printf(
        "LED TEST pattern=%s remaining=%llums brightness=%u/255 runs=%lu renders=%lu cancels=%lu",
        commissioningPatternName(
            commissioningPattern),
        static_cast<unsigned long long>(
            remainingMs),
        static_cast<unsigned>(
            ledEngine.brightness()),
        static_cast<unsigned long>(
            commissioningRuns),
        static_cast<unsigned long>(
            commissioningRenders),
        static_cast<unsigned long>(
            commissioningCancels));

    if (commissioningPattern ==
        ambilight::
            LedCommissioningPattern::
                LogicalRange) {

        Serial.printf(
            " side=%u start=%u count=%u",
            static_cast<unsigned>(
                commissioningRangeSegment),
            static_cast<unsigned>(
                commissioningRangeStart),
            static_cast<unsigned>(
                commissioningRangeCount));
    } else if (
        commissioningPattern ==
            ambilight::
                LedCommissioningPattern::
                    RawPhysicalRange) {

        Serial.printf(
            " gpio=%u start=%u count=%u",
            static_cast<unsigned>(
                ambilight::config::kLedGpios[
                    commissioningRawLane]),
            static_cast<unsigned>(
                commissioningRangeStart),
            static_cast<unsigned>(
                commissioningRangeCount));
    }

    Serial.println();
}

void finishCommissioning(
    std::uint64_t nowUs,
    bool cancelled,
    const char* reason) {

    if (commissioningPattern ==
        ambilight::LedCommissioningPattern::None) {
        return;
    }

    commissioningPattern =
        ambilight::LedCommissioningPattern::None;
    commissioningUntilUs = 0;
    commissioningDirty = false;

    if (cancelled) {
        ++commissioningCancels;
    }

    // Pull the newest DDP frame that accumulated while the test owned the
    // physical output. If none exists, restore any older cached frame.
    refreshRgbCache();

    if (rgbFrameValid) {
        rgbDirty = true;
    } else {
        ambilight::RgbFrame black;
        black.clear();
        black.pixelCount =
            runtimeSettings.
                ledMappingProfile().
                totalLedCount();
        black.receivedUs = nowUs;

        const esp_err_t result =
            renderer.render(
                black,
                ambilight::RenderGainContext::unity(runtimeSettings.ledMappingProfile()),
                ambilight::CorrectionMode::Disabled);

        if (result != ESP_OK) {
            fatal(
                "commissioning blackout failed",
                result);
        }
    }

    Serial.printf(
        "LED TEST stopped: %s.\n",
        reason != nullptr
            ? reason
            : "done");
}

void startCommissioning(
    ambilight::LedCommissioningPattern pattern) {

    const std::uint8_t brightness =
        ledEngine.brightness();

    if (brightness == 0 ||
        brightness >
            kCommissioningMaxBrightness) {

        Serial.printf(
            "LED TEST refused: set brightness to 1..%u first. Current=%u.\n",
            static_cast<unsigned>(
                kCommissioningMaxBrightness),
            static_cast<unsigned>(
                brightness));
        return;
    }

    if (pattern ==
        ambilight::LedCommissioningPattern::None) {
        return;
    }

    const std::uint64_t nowUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    ambilight::LedCommissioningPatternBuilder::build(
        pattern,
        runtimeSettings.
            ledMappingProfile(),
        commissioningFrame);

    commissioningFrame.receivedUs =
        nowUs;

    // Keep commissioning correction-independent without constructing a
    // full 920-entry unity gain field on the loop stack.
    commissioningGainContext.topology =
        runtimeSettings.ledMappingProfile();
    commissioningGainContext.sourcePresent = false;
    commissioningGainContext.sourceUsable = false;
    commissioningGainContext.failOpen = true;

    commissioningPattern = pattern;
    commissioningUntilUs =
        nowUs +
        kCommissioningDurationUs;

    commissioningDirty = true;
    ++commissioningRuns;

    Serial.printf(
        "LED TEST started: %s for %llums at brightness=%u/255. ToF correction is bypassed for the test frame.\n",
        commissioningPatternName(
            pattern),
        static_cast<unsigned long long>(
            kCommissioningDurationUs /
            1000ULL),
        static_cast<unsigned>(
            brightness));
}

bool tofDebugActive() {
    const std::uint64_t nowUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    return
        tof.debugMode() &&
        tofDebugUntilUs > nowUs;
}

void stopTofDebug(
    const char* reason) {

    const bool wasActive =
        tof.debugMode();

    tof.setDebugMode(
        false);

    tofDebugUntilUs = 0;

    if (wasActive) {
        Serial.printf(
            "TOF DEBUG stopped%s%s. Normal ~12s pose cadence restored.\n",
            reason != nullptr
                ? ": "
                : "",
            reason != nullptr
                ? reason
                : "");
    }
}

bool startTofDebug() {
    ambilight::TofSnapshot snapshot;

    if (!tof.copySnapshot(
            snapshot)) {

        Serial.println(
            "TOF DEBUG refused: ToF service is not initialized.");
        return false;
    }

    if (correctionMode ==
        ambilight::
            CorrectionMode::
                Active) {

        Serial.println(
            "TOF DEBUG refused: switch correction out of ACTIVE first.");
        return false;
    }

    const std::uint64_t nowUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    tofDebugUntilUs =
        nowUs +
        kTofDebugDurationUs;

    tof.setDebugMode(
        true);

    Serial.printf(
        "TOF DEBUG started for %llums. ToF processing cadence is ~1s; correction remains observational.\n",
        static_cast<unsigned long long>(
            kTofDebugDurationUs /
            1000ULL));

    return true;
}

void toggleTofDebug() {
    if (tofDebugActive()) {
        stopTofDebug(
            "serial toggle");
        return;
    }

    startTofDebug();
}

void serviceTofDebug(
    std::uint64_t nowUs) {

    if (!tof.debugMode()) {
        return;
    }

    if (correctionMode ==
        ambilight::
            CorrectionMode::
                Active) {

        stopTofDebug(
            "ACTIVE correction entered");
        return;
    }

    if (tofDebugUntilUs == 0 ||
        nowUs >=
            tofDebugUntilUs) {

        stopTofDebug(
            "60s timeout");
    }
}

bool commissioningBrightnessSafe() {
    const std::uint8_t brightness =
        ledEngine.brightness();

    return
        brightness > 0 &&
        brightness <=
            kCommissioningMaxBrightness;
}

bool startLogicalRangeCommissioning(
    ambilight::SegmentId segment,
    std::uint16_t start,
    std::uint16_t count) {

    if (!commissioningBrightnessSafe()) {
        Serial.printf(
            "LED RANGE refused: brightness must be 1..%u.\n",
            static_cast<unsigned>(
                kCommissioningMaxBrightness));
        return false;
    }

    if (!ambilight::
            LedCommissioningPatternBuilder::
                buildLogicalRange(
                    runtimeSettings.
                        ledMappingProfile(),
                    segment,
                    start,
                    count,
                    commissioningFrame)) {

        Serial.println(
            "LED RANGE refused: logical side/start/count are outside active topology.");
        return false;
    }

    const std::uint64_t nowUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    commissioningPattern =
        ambilight::
            LedCommissioningPattern::
                LogicalRange;

    commissioningRangeSegment =
        segment;

    commissioningRangeStart = start;
    commissioningRangeCount = count;

    commissioningUntilUs =
        nowUs +
        kCommissioningDurationUs;

    commissioningDirty = true;
    ++commissioningRuns;

    Serial.printf(
        "LED RANGE logical side=%u start=%u count=%u for %llums.\n",
        static_cast<unsigned>(
            segment),
        static_cast<unsigned>(
            start),
        static_cast<unsigned>(
            count),
        static_cast<unsigned long long>(
            kCommissioningDurationUs /
            1000ULL));

    return true;
}

bool startRawGpioCommissioning(
    std::uint8_t gpio,
    std::uint16_t start,
    std::uint16_t count) {

    if (!commissioningBrightnessSafe()) {
        Serial.printf(
            "GPIO TEST refused: brightness must be 1..%u.\n",
            static_cast<unsigned>(
                kCommissioningMaxBrightness));
        return false;
    }

    std::uint8_t lane = 0;

    if (!ambilight::
            LedMappingProfile::
                laneForGpio(
                    gpio,
                    lane)) {

        Serial.println(
            "GPIO TEST refused: allowed GPIO are 18,19,20,21.");
        return false;
    }

    if (count == 0 ||
        start >=
            ambilight::config::
                kPhysicalLaneLength ||
        static_cast<std::uint32_t>(
            start) +
            count >
                ambilight::config::
                    kPhysicalLaneLength) {

        Serial.println(
            "GPIO TEST refused: start/count exceed physical 230-address lane.");
        return false;
    }

    const std::uint64_t nowUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    commissioningPattern =
        ambilight::
            LedCommissioningPattern::
                RawPhysicalRange;

    commissioningRawLane = lane;
    commissioningRangeStart = start;
    commissioningRangeCount = count;

    commissioningUntilUs =
        nowUs +
        kCommissioningDurationUs;

    commissioningDirty = true;
    ++commissioningRuns;

    Serial.printf(
        "GPIO TEST GPIO%u lane=%u start=%u count=%u for %llums; logical mapping bypassed.\n",
        static_cast<unsigned>(
            gpio),
        static_cast<unsigned>(
            lane),
        static_cast<unsigned>(
            start),
        static_cast<unsigned>(
            count),
        static_cast<unsigned long long>(
            kCommissioningDurationUs /
            1000ULL));

    return true;
}

bool handleCommissioningRangePayload(
    const char* command) {

    ambilight::CommissioningRangeRequest request;

    const auto parsed =
        ambilight::RuntimePayloadParser::
            parseCommissioningRange(
                command,
                request);

    if (parsed !=
        ambilight::RuntimePayloadParseResult::Ok) {

        Serial.println(
            "LED RANGE invalid. Use side:SIDE:START:COUNT or gpio:GPIO:START:COUNT.");
        return false;
    }

    if (request.target ==
        ambilight::
            CommissioningRangeTarget::
                LogicalSide) {

        return
            startLogicalRangeCommissioning(
                static_cast<
                    ambilight::SegmentId>(
                        request.targetValue),
                request.start,
                request.count);
    }

    return
        startRawGpioCommissioning(
            request.targetValue,
            request.start,
            request.count);
}

bool serviceCommissioning(
    std::uint64_t nowUs) {

    if (commissioningPattern ==
        ambilight::LedCommissioningPattern::None) {
        return false;
    }

    if (nowUs >=
        commissioningUntilUs) {

        finishCommissioning(
            nowUs,
            false,
            "duration complete");

        return false;
    }

    const std::uint8_t brightness =
        ledEngine.brightness();

    if (brightness == 0 ||
        brightness >
            kCommissioningMaxBrightness) {

        finishCommissioning(
            nowUs,
            true,
            "brightness left safe test range");

        return false;
    }

    if (!commissioningDirty &&
        !brightnessDirty &&
        !ledMappingDirty &&
        !ledPixelMaskDirty) {

        return true;
    }

    commissioningFrame.receivedUs =
        nowUs;

    esp_err_t result = ESP_OK;
    const bool firstDiagnosticRender =
        commissioningDirty;

    if (firstDiagnosticRender) {
        printLedRuntimeProbe(
            "commissioning-before-output");
    }

    if (commissioningPattern ==
        ambilight::
            LedCommissioningPattern::
                RawPhysicalRange) {

        ledEngine.clear();

        for (std::uint16_t offset = 0;
             offset <
                commissioningRangeCount;
             ++offset) {

            if (!ledEngine.setPhysicalPixel(
                    commissioningRawLane,
                    static_cast<std::uint16_t>(
                        commissioningRangeStart +
                        offset),
                    static_cast<crgb_t>(
                        0x00FFFFFFU))) {

                result = ESP_FAIL;
                break;
            }
        }

        if (result == ESP_OK) {
            if (firstDiagnosticRender) {
                Serial.println(
                    "LED DIAG breadcrumb=raw-before-parlio-show");
                Serial.flush();
            }

            result =
                ledEngine.show();

            if (firstDiagnosticRender) {
                Serial.printf(
                    "LED DIAG breadcrumb=raw-after-parlio-show result=%s show_us=%lu\n",
                    esp_err_to_name(result),
                    static_cast<unsigned long>(
                        ledEngine.lastShowTimeUs()));
            }
        }
    } else {
        if (firstDiagnosticRender) {
            Serial.println(
                "LED DIAG breadcrumb=logical-before-render");
            Serial.flush();
        }

        result =
            renderer.render(
                commissioningFrame,
                commissioningGainContext,
                ambilight::CorrectionMode::Disabled);

        if (firstDiagnosticRender) {
            Serial.printf(
                "LED DIAG breadcrumb=logical-after-render result=%s show_us=%lu prepare_us=%lu\n",
                esp_err_to_name(result),
                static_cast<unsigned long>(
                    ledEngine.lastShowTimeUs()),
                static_cast<unsigned long>(
                    renderer.shadowStats().lastPrepareUs));
        }
    }

    if (firstDiagnosticRender) {
        printLedRuntimeProbe(
            "commissioning-after-output");
    }

    if (result != ESP_OK) {
        fatal(
            "commissioning render failed",
            result);
    }

    ++commissioningRenders;
    commissioningDirty = false;
    brightnessDirty = false;
    ledMappingDirty = false;
    ledPixelMaskDirty = false;

    return true;
}

void refreshTargetGainContext(
    std::uint64_t nowUs) {

    if (nextGainTargetPollUs != 0 &&
        nowUs < nextGainTargetPollUs) {
        return;
    }

    if (!ambilight::CorrectionOutputPolicy::evaluatesGain(
            correctionMode)) {

        cachedTargetGainContext =
            ambilight::RenderGainContext::unity(runtimeSettings.ledMappingProfile());

        nextGainTargetPollUs =
            nowUs + kGainTargetPollIntervalUs;

        return;
    }

    ambilight::PerimeterGainSnapshot latest{};

    if (tof.copyPerimeterGainSnapshot(latest)) {
        cachedPerimeterGainSnapshot = latest;
        haveCachedPerimeterGainSnapshot = true;
    }

    cachedTargetGainContext =
        ambilight::TofRenderGainBridge::make(
            cachedPerimeterGainSnapshot,
            haveCachedPerimeterGainSnapshot,
            nowUs);

    if (shadowProbeUntilUs != 0 &&
        nowUs < shadowProbeUntilUs &&
        correctionMode !=
            ambilight::CorrectionMode::Active) {

        cachedTargetGainContext =
            ambilight::ShadowGainProbe::make(
                shadowProbeGeneration,
                nowUs,
                runtimeSettings.
                    ledMappingProfile());
    }

    nextGainTargetPollUs =
        nowUs + kGainTargetPollIntervalUs;
}

bool serviceRender(std::uint64_t nowUs) {
    refreshRgbCache();
    refreshTargetGainContext(nowUs);

    const bool gainPipelineEnabled =
        ambilight::CorrectionOutputPolicy::evaluatesGain(
            correctionMode);

    const bool targetProfileChanged =
        gainPipelineEnabled &&
        !cachedTargetGainContext.sameRenderProfileAs(
            renderGainController.target());

    const bool renderStateDirty =
        correctionModeDirty ||
        brightnessDirty ||
        ledMappingDirty ||
        ledPixelMaskDirty ||
        (
            gainPipelineEnabled &&
            (
                targetProfileChanged ||
                !renderGainController.settled()
            )
        );

    const ambilight::RenderDecision decision =
        renderScheduler.decide(
            rgbFrameValid,
            rgbDirty,
            renderStateDirty,
            nowUs);

    if (!decision.render) {
        return false;
    }

    if (decision.dueToRgb) {
        lastFrameAgeUs =
            nowUs >= renderSnapshot.receivedUs
                ? nowUs - renderSnapshot.receivedUs
                : 0;

        if (lastFrameAgeUs >
            maxFrameAgeUs) {
            maxFrameAgeUs =
                lastFrameAgeUs;
        }

        // Only a genuinely new DDP frame belongs in the transport latency
        // histogram. State-only rerenders intentionally reuse an old RGB frame
        // and must not look like network queue latency.
        if (renderSnapshot.receivedUs != 0 &&
            renderSnapshot.receivedUs ==
                ddp.lastCompleteFrameUs()) {

            frameAgeHistogram.observe(
                lastFrameAgeUs);
        }
    }

    ambilight::RenderGainContext effectiveGainContext =
        ambilight::RenderGainContext::unity(runtimeSettings.ledMappingProfile());

    if (gainPipelineEnabled) {
        effectiveGainContext =
            renderGainController.update(
                cachedTargetGainContext,
                nowUs);
    }

    const esp_err_t result =
        renderer.render(
            renderSnapshot,
            effectiveGainContext,
            correctionMode);

    if (result != ESP_OK) {
        fatal(
            "LedRenderer::render failed",
            result);
    }

    renderScheduler.markRendered(nowUs);
    correctionModeDirty = false;
    brightnessDirty = false;
    ledMappingDirty = false;
    ledPixelMaskDirty = false;

    if (decision.dueToRgb) {
        rgbDirty = false;
        lastRenderedRgbGeneration =
            renderSnapshot.generation;
    }

    return true;
}

void serviceIdleBlackout(std::uint64_t nowUs) {
    const std::uint64_t lastCompleteUs = ddp.lastCompleteFrameUs();

    if (lastCompleteUs == 0 ||
        nowUs - lastCompleteUs <= kIdleBlackoutUs ||
        idleBlanked) {
        return;
    }

    ambilight::RgbFrame black;
    black.clear();
    black.pixelCount =
        runtimeSettings.
            ledMappingProfile().
            totalLedCount();
    black.receivedUs = nowUs;

    if (!mailbox.publish(black)) {
        fatal("idle blackout publish failed", ESP_FAIL);
    }

    idleBlanked = true;
    ++idleBlackouts;
}

void updateDdpActivityState() {
    const std::uint32_t publications =
        ddp.stats().mailboxPublications;

    if (publications != lastObservedPublications) {
        lastObservedPublications = publications;
        idleBlanked = false;
    }
}

void dumpTofMap() {
    ambilight::TofSnapshot snapshot;

    if (!tof.copySnapshot(snapshot)) {
        Serial.println(
            "TOF dump unavailable: snapshot mutex busy/not initialized.");
        return;
    }

    Serial.printf(
        "TOF RAW state=%s gen=%lu age_ms=%llu valid=%u median=%umm "
        "frames=%lu read=%luus max_read=%luus\n",
        tofStateName(snapshot.state),
        static_cast<unsigned long>(snapshot.generation),
        snapshot.timestampUs == 0
            ? 0ULL
            : static_cast<unsigned long long>(
                  (static_cast<std::uint64_t>(esp_timer_get_time()) -
                   snapshot.timestampUs) / 1000ULL),
        static_cast<unsigned>(snapshot.validZones),
        snapshot.medianMm,
        static_cast<unsigned long>(snapshot.frames),
        static_cast<unsigned long>(snapshot.lastReadUs),
        static_cast<unsigned long>(snapshot.maxReadUs));

    Serial.println("distance_mm/status, raw ST zone order:");

    for (std::size_t row = 0; row < 8; ++row) {
        for (std::size_t col = 0; col < 8; ++col) {
            const std::size_t index = row * 8 + col;

            Serial.printf(
                "%5d/%02u%s",
                static_cast<int>(snapshot.distanceMm[index]),
                static_cast<unsigned>(snapshot.targetStatus[index]),
                col == 7 ? "" : " ");
        }

        Serial.println();
    }

    Serial.println();
}

void printBand(
    const char* name,
    const ambilight::TofBandEstimate& band) {

    Serial.printf(
        "%s valid=%s candidates=%u accepted=%u raw=%umm mad=%umm robust=%umm filtered=%umm\n",
        name,
        band.valid ? "yes" : "no",
        static_cast<unsigned>(band.candidates),
        static_cast<unsigned>(band.accepted),
        band.rawMedianMm,
        band.madMm,
        band.robustMedianMm,
        band.filteredMm);
}

void dumpTofGeometry() {
    ambilight::TofSnapshot snapshot;

    if (!tof.copySnapshot(snapshot)) {
        Serial.println(
            "TOF geometry unavailable: snapshot mutex busy/not initialized.");
        return;
    }

    const auto& geometry = snapshot.geometry;

    Serial.printf(
        "TOF GEOMETRY state=%s raw_gen=%lu geom_gen=%lu valid=%s accepted=%u "
        "right_minus_left=%dmm rotation=%u mirror_x=%s\n",
        tofStateName(snapshot.state),
        static_cast<unsigned long>(snapshot.generation),
        static_cast<unsigned long>(geometry.generation),
        geometry.valid ? "yes" : "no",
        static_cast<unsigned>(geometry.acceptedZones),
        static_cast<int>(geometry.rightMinusLeftMm),
        static_cast<unsigned>(
            runtimeSettings
                .tofSpatialProfile()
                .rotationQuarterTurns),
        runtimeSettings
                .tofSpatialProfile()
                .mirrorX
            ? "yes"
            : "no");

    printBand("LEFT  ", geometry.left);
    printBand("CENTER", geometry.center);
    printBand("RIGHT ", geometry.right);

    Serial.println();
}

void dumpTofPlane() {
    ambilight::TofSnapshot snapshot;

    if (!tof.copySnapshot(snapshot)) {
        Serial.println(
            "TOF plane unavailable: snapshot mutex busy/not initialized.");
        return;
    }

    const auto& plane =
        snapshot.geometry.plane;

    Serial.printf(
        "TOF PLANE valid=%s candidates=%u accepted=%u z0=%.1fmm "
        "slope_x=%.5f slope_y=%.5f yaw=%.2fdeg pitch=%.2fdeg "
        "residual_med=%umm residual_mad=%umm observed_half=%ux%umm\n",
        plane.valid ? "yes" : "no",
        static_cast<unsigned>(plane.candidates),
        static_cast<unsigned>(plane.accepted),
        static_cast<double>(plane.interceptMm),
        static_cast<double>(plane.slopeX),
        static_cast<double>(plane.slopeY),
        static_cast<double>(plane.yawCentiDeg) / 100.0,
        static_cast<double>(plane.pitchCentiDeg) / 100.0,
        plane.residualMedianMm,
        plane.residualMadMm,
        plane.observedHalfSpanXmm,
        plane.observedHalfSpanYmm);

    Serial.println(
        "Plane coordinates: +X right, +Y up, +Z toward wall. "
        "Positive yaw means wall farther on right; positive pitch means farther at top.");
    Serial.println();
}

std::uint32_t gainPercentX10(std::uint16_t gainQ12) {
    return static_cast<std::uint32_t>(
        (static_cast<std::uint32_t>(gainQ12) * 1000U +
         ambilight::kGainUnityQ12 / 2U) /
        ambilight::kGainUnityQ12);
}

void printRenderSegmentGain(
    const char* name,
    ambilight::SegmentId segment,
    const ambilight::RenderGainContext& context) {

    const auto endpoints =
        context.endpointsForSegment(segment);

    std::uint16_t midpointQ12 =
        ambilight::kGainUnityQ12;

    const auto segmentConfig =
        context.topology.segmentConfig(
            segment);

    if (segmentConfig.logicalLength > 0) {
        const std::uint16_t midpoint =
            static_cast<std::uint16_t>(
                segmentConfig.logicalStart +
                segmentConfig.logicalLength / 2U);

        midpointQ12 =
            context.gainForLogicalIndex(
                midpoint);
    }

    Serial.printf(
        "RENDER %s start=%u(%lu.%lu%%) mid=%u(%lu.%lu%%) end=%u(%lu.%lu%%)\n",
        name,
        endpoints.startQ12,
        static_cast<unsigned long>(
            gainPercentX10(endpoints.startQ12) / 10U),
        static_cast<unsigned long>(
            gainPercentX10(endpoints.startQ12) % 10U),
        midpointQ12,
        static_cast<unsigned long>(
            gainPercentX10(midpointQ12) / 10U),
        static_cast<unsigned long>(
            gainPercentX10(midpointQ12) % 10U),
        endpoints.endQ12,
        static_cast<unsigned long>(
            gainPercentX10(endpoints.endQ12) / 10U),
        static_cast<unsigned long>(
            gainPercentX10(endpoints.endQ12) % 10U));
}

void dumpRenderShadow() {
    const auto& stats =
        renderer.shadowStats();

    const auto& context =
        renderer.lastGainContext();

    const auto& controllerStats =
        renderGainController.stats();

    const auto& schedulerStats =
        renderScheduler.stats();

    const auto& targetContext =
        renderGainController.target();

    const std::uint32_t shadowPermille =
        stats.lastInputChannelSum == 0
            ? 1000U
            : static_cast<std::uint32_t>(
                  (static_cast<std::uint64_t>(
                       stats.lastShadowChannelSum) *
                       1000ULL +
                   stats.lastInputChannelSum / 2U) /
                  stats.lastInputChannelSum);

    Serial.printf(
        "RENDER mode=%s frames=%lu disabled=%lu shadow=%lu active=%lu present=%lu usable=%lu failopen=%lu nonunity=%lu "
        "last_candidate_changed=%u last_physical_changed=%u max_delta=%u candidate_rgb=%lu.%lu%% source_gen=%lu source_age=%lluus "
        "prepare=%luus prepare_max=%luus\n",
        ambilight::correctionModeName(
            renderer.lastCorrectionMode()),
        static_cast<unsigned long>(stats.frames),
        static_cast<unsigned long>(stats.disabledFrames),
        static_cast<unsigned long>(stats.shadowFrames),
        static_cast<unsigned long>(stats.activeFrames),
        static_cast<unsigned long>(stats.sourcePresentFrames),
        static_cast<unsigned long>(stats.sourceUsableFrames),
        static_cast<unsigned long>(stats.failOpenFrames),
        static_cast<unsigned long>(stats.nonUnityContextFrames),
        stats.lastWouldChangePixels,
        stats.lastPhysicalChangedPixels,
        static_cast<unsigned>(stats.lastMaxChannelDelta),
        static_cast<unsigned long>(shadowPermille / 10U),
        static_cast<unsigned long>(shadowPermille % 10U),
        static_cast<unsigned long>(stats.lastSourceGeneration),
        static_cast<unsigned long long>(stats.lastSourceAgeUs),
        static_cast<unsigned long>(stats.lastPrepareUs),
        static_cast<unsigned long>(stats.maxPrepareUs));

    Serial.printf(
        "RENDER cumulative_candidate_changed=%llu cumulative_physical_changed=%llu max_delta_ever=%u "
        "candidate_by_segment T=%llu R=%llu B=%llu L=%llu\n",
        static_cast<unsigned long long>(
            stats.wouldChangePixels),
        static_cast<unsigned long long>(
            stats.physicalChangedPixels),
        static_cast<unsigned>(stats.maxChannelDelta),
        static_cast<unsigned long long>(
            stats.wouldChangeBySegment[
                static_cast<std::size_t>(
                    ambilight::SegmentId::Top)]),
        static_cast<unsigned long long>(
            stats.wouldChangeBySegment[
                static_cast<std::size_t>(
                    ambilight::SegmentId::Right)]),
        static_cast<unsigned long long>(
            stats.wouldChangeBySegment[
                static_cast<std::size_t>(
                    ambilight::SegmentId::Bottom)]),
        static_cast<unsigned long long>(
            stats.wouldChangeBySegment[
                static_cast<std::size_t>(
                    ambilight::SegmentId::Left)]));

    Serial.printf(
        "RENDER CONTEXT effective present=%s usable=%s failopen=%s nonunity=%s probe=%s\n",
        context.sourcePresent ? "yes" : "no",
        context.sourceUsable ? "yes" : "no",
        context.failOpen ? "yes" : "no",
        context.hasNonUnityGain() ? "yes" : "no",
        shadowGainProbeActive() ? "yes" : "no");

    Serial.printf(
        "RENDER TARGET present=%s usable=%s failopen=%s nonunity=%s gen=%lu age=%lluus\n",
        targetContext.sourcePresent ? "yes" : "no",
        targetContext.sourceUsable ? "yes" : "no",
        targetContext.failOpen ? "yes" : "no",
        targetContext.hasNonUnityGain() ? "yes" : "no",
        static_cast<unsigned long>(targetContext.sourceGeneration),
        static_cast<unsigned long long>(targetContext.sourceAgeUs));

    printRenderSegmentGain(
        "TGT TOP   ",
        ambilight::SegmentId::Top,
        targetContext);
    printRenderSegmentGain(
        "TGT RIGHT ",
        ambilight::SegmentId::Right,
        targetContext);
    printRenderSegmentGain(
        "TGT BOTTOM",
        ambilight::SegmentId::Bottom,
        targetContext);
    printRenderSegmentGain(
        "TGT LEFT  ",
        ambilight::SegmentId::Left,
        targetContext);

    Serial.printf(
        "RENDER SCHED rgb=%lu state_only=%lu combined=%lu state_deferred=%lu no_frame=%lu clean=%lu "
        "last_rgb_gen=%lu mailbox_gen=%lu\n",
        static_cast<unsigned long>(schedulerStats.rgbRenders),
        static_cast<unsigned long>(schedulerStats.stateOnlyRenders),
        static_cast<unsigned long>(schedulerStats.combinedRenders),
        static_cast<unsigned long>(schedulerStats.stateDeferrals),
        static_cast<unsigned long>(schedulerStats.noFrameSkips),
        static_cast<unsigned long>(schedulerStats.cleanSkips),
        static_cast<unsigned long>(lastRenderedRgbGeneration),
        static_cast<unsigned long>(lastMailboxGeneration));

    Serial.printf(
        "RENDER SLEW updates=%lu usable_targets=%lu failopen_targets=%lu gen_changes=%lu "
        "unity_snaps=%lu time_rollbacks=%lu max_step=%u\n",
        static_cast<unsigned long>(controllerStats.updates),
        static_cast<unsigned long>(controllerStats.usableTargets),
        static_cast<unsigned long>(controllerStats.failOpenTargets),
        static_cast<unsigned long>(controllerStats.targetGenerationChanges),
        static_cast<unsigned long>(controllerStats.failOpenUnitySnaps),
        static_cast<unsigned long>(controllerStats.timeRollbacks),
        static_cast<unsigned>(controllerStats.maxPixelStepQ12));

    printRenderSegmentGain(
        "TOP   ",
        ambilight::SegmentId::Top,
        context);

    printRenderSegmentGain(
        "RIGHT ",
        ambilight::SegmentId::Right,
        context);

    printRenderSegmentGain(
        "BOTTOM",
        ambilight::SegmentId::Bottom,
        context);

    printRenderSegmentGain(
        "LEFT  ",
        ambilight::SegmentId::Left,
        context);

    Serial.printf(
        "RENDER OUTPUT: %s. %s\n",
        ambilight::correctionModeName(
            correctionMode),
        correctionMode ==
                ambilight::CorrectionMode::Active
            ? "candidate RGB may reach physical LEDs; fail-open still resolves to original RGB"
            : "physical LEDs receive original HyperHDR RGB");
    Serial.println();
}

void printPerimeterSegmentGain(
    const char* name,
    ambilight::SegmentId segmentId,
    const ambilight::PerimeterGainSnapshot& gains) {

    const auto index =
        static_cast<std::size_t>(
            segmentId);

    const auto& segment =
        gains.segment[index];

    std::uint16_t midpointQ12 =
        ambilight::kGainUnityQ12;

    const auto segmentConfig =
        gains.topology.segmentConfig(
            segmentId);

    if (segmentConfig.logicalLength > 0) {
        midpointQ12 =
            gains.logicalGainQ12[
                segmentConfig.logicalStart +
                segmentConfig.logicalLength / 2U];
    }

    Serial.printf(
        "TOF SPATIAL %s d=%u->%umm k=%u(%lu.%lu%%) mid=%u(%lu.%lu%%) ->%u(%lu.%lu%%)\n",
        name,
        segment.startDistanceMm,
        segment.endDistanceMm,
        segment.startQ12,
        static_cast<unsigned long>(
            gainPercentX10(segment.startQ12) / 10U),
        static_cast<unsigned long>(
            gainPercentX10(segment.startQ12) % 10U),
        midpointQ12,
        static_cast<unsigned long>(
            gainPercentX10(midpointQ12) / 10U),
        static_cast<unsigned long>(
            gainPercentX10(midpointQ12) % 10U),
        segment.endQ12,
        static_cast<unsigned long>(
            gainPercentX10(segment.endQ12) / 10U),
        static_cast<unsigned long>(
            gainPercentX10(segment.endQ12) % 10U));
}

void dumpSpatialGains() {
    ambilight::TofSnapshot snapshot;

    if (!tof.copySnapshot(snapshot)) {
        Serial.println(
            "TOF spatial gains unavailable: snapshot mutex busy/not initialized.");
        return;
    }

    const auto& gains =
        snapshot.perimeterGains;

    Serial.printf(
        "TOF SPATIAL GAINS gen=%lu plane_usable=%s projection_usable=%s fail_open=%s "
        "range=%u..%umm\n",
        static_cast<unsigned long>(gains.generation),
        gains.planeUsable ? "yes" : "no",
        gains.projectionUsable ? "yes" : "no",
        gains.failOpen ? "yes" : "no",
        gains.minDistanceMm,
        gains.maxDistanceMm);

    printPerimeterSegmentGain(
        "TOP   ",
        ambilight::SegmentId::Top,
        gains);

    printPerimeterSegmentGain(
        "RIGHT ",
        ambilight::SegmentId::Right,
        gains);

    printPerimeterSegmentGain(
        "BOTTOM",
        ambilight::SegmentId::Bottom,
        gains);

    printPerimeterSegmentGain(
        "LEFT  ",
        ambilight::SegmentId::Left,
        gains);

    Serial.printf(
        "These are ToF correction targets; physical application depends on correction mode=%s.\n",
        ambilight::correctionModeName(
            correctionMode));
    Serial.println();
}

void dumpTofGains() {
    ambilight::TofSnapshot snapshot;

    if (!tof.copySnapshot(snapshot)) {
        Serial.println(
            "TOF gain snapshot unavailable: snapshot mutex busy/not initialized.");
        return;
    }

    const auto& gains = snapshot.gains;

    Serial.printf(
        "TOF GAINS gen=%lu geometry_usable=%s fail_open=%s "
        "L=%u(%lu.%lu%%) R=%u(%lu.%lu%%) T=%u(%lu.%lu%%) B=%u(%lu.%lu%%)\n",
        static_cast<unsigned long>(gains.generation),
        gains.geometryUsable ? "yes" : "no",
        gains.failOpen ? "yes" : "no",
        gains.leftQ12,
        static_cast<unsigned long>(gainPercentX10(gains.leftQ12) / 10U),
        static_cast<unsigned long>(gainPercentX10(gains.leftQ12) % 10U),
        gains.rightQ12,
        static_cast<unsigned long>(gainPercentX10(gains.rightQ12) / 10U),
        static_cast<unsigned long>(gainPercentX10(gains.rightQ12) % 10U),
        gains.topQ12,
        static_cast<unsigned long>(gainPercentX10(gains.topQ12) / 10U),
        static_cast<unsigned long>(gainPercentX10(gains.topQ12) % 10U),
        gains.bottomQ12,
        static_cast<unsigned long>(gainPercentX10(gains.bottomQ12) / 10U),
        static_cast<unsigned long>(gainPercentX10(gains.bottomQ12) % 10U));

    Serial.printf(
        "Legacy band gains are diagnostic; the renderer uses the plane/per-pixel model. Physical application mode=%s.\n",
        ambilight::correctionModeName(
            correctionMode));
    Serial.println();
}

void printCalibrationBand(
    const char* name,
    const ambilight::CalibrationBandSummary& band) {

    Serial.printf(
        "CAL %s valid=%s p10=%umm median=%umm p90=%umm mad50=%umm accepted=%u..%u\n",
        name,
        band.valid ? "yes" : "no",
        band.p10Mm,
        band.medianMm,
        band.p90Mm,
        band.medianMadMm,
        static_cast<unsigned>(band.minAccepted),
        static_cast<unsigned>(band.maxAccepted));
}

void printCalibrationSummary(
    const ambilight::CalibrationCaptureSummary& summary) {

    Serial.printf(
        "CAL SUMMARY total=%lu valid=%lu duplicates=%lu overflow=%lu delta_median=%dmm\n",
        static_cast<unsigned long>(summary.totalFrames),
        static_cast<unsigned long>(summary.validFrames),
        static_cast<unsigned long>(summary.duplicateFrames),
        static_cast<unsigned long>(summary.overflowFrames),
        static_cast<int>(summary.medianRightMinusLeftMm));

    printCalibrationBand("LEFT  ", summary.left);
    printCalibrationBand("CENTER", summary.center);
    printCalibrationBand("RIGHT ", summary.right);

    if (summary.plane.valid) {
        Serial.printf(
            "CAL PLANE frames=%lu yaw=%.2f/%.2f/%.2fdeg pitch=%.2f/%.2f/%.2fdeg "
            "z0=%u/%u/%umm residual_mad50=%umm accepted=%u..%u\n",
            static_cast<unsigned long>(summary.plane.validFrames),
            static_cast<double>(summary.plane.yawCentiDeg.p10) / 100.0,
            static_cast<double>(summary.plane.yawCentiDeg.median) / 100.0,
            static_cast<double>(summary.plane.yawCentiDeg.p90) / 100.0,
            static_cast<double>(summary.plane.pitchCentiDeg.p10) / 100.0,
            static_cast<double>(summary.plane.pitchCentiDeg.median) / 100.0,
            static_cast<double>(summary.plane.pitchCentiDeg.p90) / 100.0,
            summary.plane.interceptMm.p10,
            summary.plane.interceptMm.median,
            summary.plane.interceptMm.p90,
            summary.plane.medianResidualMadMm,
            static_cast<unsigned>(summary.plane.minAccepted),
            static_cast<unsigned>(summary.plane.maxAccepted));
    }

    if (summary.spatial.valid) {
        Serial.printf(
            "CAL WALL DIST frames=%lu min=%u/%u/%umm max=%u/%u/%umm\n",
            static_cast<unsigned long>(summary.spatial.validFrames),
            summary.spatial.minDistanceMm.p10,
            summary.spatial.minDistanceMm.median,
            summary.spatial.minDistanceMm.p90,
            summary.spatial.maxDistanceMm.p10,
            summary.spatial.maxDistanceMm.median,
            summary.spatial.maxDistanceMm.p90);

        static constexpr const char* kNames[] = {
            "TOP", "RIGHT", "BOTTOM", "LEFT"
        };

        for (std::size_t index = 0;
             index < summary.spatial.segment.size();
             ++index) {

            const auto& segment =
                summary.spatial.segment[index];

            Serial.printf(
                "CAL WALL %s start=%u/%u/%umm end=%u/%u/%umm\n",
                kNames[index],
                segment.startMm.p10,
                segment.startMm.median,
                segment.startMm.p90,
                segment.endMm.p10,
                segment.endMm.median,
                segment.endMm.p90);
        }
    }

    Serial.println(
        "CAL note: capture is observational only and does not change correction mode or RGB state.");
    Serial.println();
}

void startCalibrationCapture() {
    const std::uint64_t nowUs =
        static_cast<std::uint64_t>(esp_timer_get_time());

    calibrationLastGeometryGeneration = 0;
    calibrationCapture.start(nowUs);

    Serial.println(
        "CAL started: collecting 60 seconds of slow ToF pose samples. Keep the TV still.");
}

void serviceCalibrationCapture() {
    if (!calibrationCapture.active()) {
        return;
    }

    ambilight::TofSnapshot snapshot;
    if (tof.copySnapshot(snapshot) &&
        snapshot.geometry.generation != 0 &&
        snapshot.geometry.generation !=
            calibrationLastGeometryGeneration) {

        calibrationLastGeometryGeneration =
            snapshot.geometry.generation;

        calibrationCapture.ingest(
            snapshot.geometry,
            snapshot.perimeterGains);
    }

    const std::uint64_t nowUs =
        static_cast<std::uint64_t>(esp_timer_get_time());

    if (!calibrationCapture.expired(nowUs)) {
        return;
    }

    const auto summary =
        calibrationCapture.finish();

    lastCalibrationSummary = summary;
    haveLastCalibrationSummary = true;

    printCalibrationSummary(summary);
}

void startShadowGainProbe() {
    if (correctionMode !=
        ambilight::CorrectionMode::Shadow) {

        Serial.println(
            "SHADOW PROBE is available only in SHADOW mode.");
        return;
    }

    const std::uint64_t nowUs =
        static_cast<std::uint64_t>(esp_timer_get_time());

    ++shadowProbeGeneration;
    if (shadowProbeGeneration == 0) {
        ++shadowProbeGeneration;
    }

    shadowProbeUntilUs =
        nowUs + 10000000ULL;

    // Force target refresh on the very next render service tick.
    nextGainTargetPollUs = 0;

    Serial.println(
        "SHADOW PROBE started for 10 seconds: aggressive per-segment gains and gradients are simulated only. Physical RGB remains original.");
}

bool shadowGainProbeActive() {
    if (shadowProbeUntilUs == 0) {
        return false;
    }

    const std::uint64_t nowUs =
        static_cast<std::uint64_t>(esp_timer_get_time());

    return nowUs < shadowProbeUntilUs;
}

template <std::size_t N>
void copyWebText(
    std::array<char, N>& destination,
    const char* source) {

    destination.fill('\0');

    if (source == nullptr ||
        N == 0) {

        return;
    }

    std::snprintf(
        destination.data(),
        destination.size(),
        "%s",
        source);
}

void recordWebAction(
    std::uint32_t sequence,
    bool ok,
    const char* message) {

    lastWebActionSequence =
        sequence;

    lastWebActionOk =
        ok;

    copyWebText(
        lastWebActionMessage,
        message != nullptr
            ? message
            : (ok ? "ok" : "failed"));
}

bool fillWebUiSnapshot(
    ambilight::WebUiSnapshot& snapshot) {

    snapshot = {};

    snapshot.persistenceAvailable =
        runtimeSettings.persistenceAvailable();

    snapshot.correctionMode =
        correctionMode;

    snapshot.brightness =
        ledEngine.brightness();

    snapshot.wifiEnabled =
        wifi.enabled();

    snapshot.wifiConnected =
        wifi.connected();

    copyWebText(
        snapshot.wifiSsid,
        wifi.ssid());

    if (snapshot.wifiConnected) {
        const String ip =
            WiFi.localIP().toString();

        copyWebText(
            snapshot.wifiIp,
            ip.c_str());

        snapshot.wifiRssi =
            WiFi.RSSI();
    }

    snapshot.ddpRunning =
        ddp.running();

    snapshot.ddpCompleteFrames =
        ddp.stats().
            completeFramesAssembled;

    snapshot.ddpPublications =
        ddp.stats().
            mailboxPublications;

    snapshot.senderLocked =
        ddp.senderLocked();

    snapshot.senderPort =
        ddp.activeSenderPort();

    if (snapshot.senderLocked) {
        in_addr address{};
        address.s_addr =
            ddp.
                activeSenderIpv4NetworkOrder();

        char senderIp[
            INET_ADDRSTRLEN] = {};

        if (inet_ntop(
                AF_INET,
                &address,
                senderIp,
                sizeof(senderIp)) !=
            nullptr) {

            copyWebText(
                snapshot.senderIp,
                senderIp);
        }
    }

    ambilight::TofSnapshot tofSnapshot;

    if (tof.copySnapshot(
            tofSnapshot)) {

        snapshot.tofAvailable = true;

        copyWebText(
            snapshot.tofState,
            tofStateName(
                tofSnapshot.state));

        snapshot.tofGeneration =
            tofSnapshot.generation;

        const std::uint64_t nowUs =
            static_cast<std::uint64_t>(
                esp_timer_get_time());

        snapshot.tofAgeMs =
            tofSnapshot.timestampUs != 0 &&
            nowUs >=
                tofSnapshot.timestampUs
                ? (
                    nowUs -
                    tofSnapshot.timestampUs
                  ) /
                    1000ULL
                : 0ULL;

        snapshot.tofValidZones =
            tofSnapshot.validZones;

        snapshot.tofMedianMm =
            tofSnapshot.medianMm;

        snapshot.planeValid =
            tofSnapshot.geometry.plane.valid;

        snapshot.planeYawCentiDeg =
            tofSnapshot
                .geometry
                .plane
                .yawCentiDeg;

        snapshot.planePitchCentiDeg =
            tofSnapshot
                .geometry
                .plane
                .pitchCentiDeg;

        snapshot.planeAccepted =
            tofSnapshot
                .geometry
                .plane
                .accepted;

        snapshot.perimeterFailOpen =
            tofSnapshot
                .perimeterGains
                .failOpen;

        snapshot.perimeterMinMm =
            tofSnapshot
                .perimeterGains
                .minDistanceMm;

        snapshot.perimeterMaxMm =
            tofSnapshot
                .perimeterGains
                .maxDistanceMm;

        ambilight::TofRawFrame raw;
        raw.timestampUs =
            tofSnapshot.timestampUs;

        raw.distanceMm =
            tofSnapshot.distanceMm;

        raw.targetStatus =
            tofSnapshot.targetStatus;

        const auto debugGrid =
            ambilight::makeTofDebugGrid(
                raw,
                runtimeSettings.
                    tofSpatialProfile().
                    transform());

        for (std::size_t index = 0;
             index <
                debugGrid.size();
             ++index) {

            snapshot.
                tofNormalizedDistanceMm[
                    index] =
                debugGrid[index].
                    distanceMm;

            snapshot.
                tofNormalizedStatus[
                    index] =
                debugGrid[index].
                    status;

            snapshot.
                tofNormalizedRawIndex[
                    index] =
                debugGrid[index].
                    rawIndex;
        }
    } else {
        copyWebText(
            snapshot.tofState,
            "unavailable");
    }

    const std::uint64_t debugNowUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    snapshot.tofDebugActive =
        tofDebugActive();

    snapshot.tofDebugRemainingMs =
        snapshot.tofDebugActive &&
        tofDebugUntilUs >
            debugNowUs
            ? static_cast<
                  std::uint32_t>(
                    (
                        tofDebugUntilUs -
                        debugNowUs
                    ) /
                    1000ULL)
            : 0U;

    snapshot.spatialProfile =
        runtimeSettings.
            tofSpatialProfile();

    snapshot.spatialCustomized =
        runtimeSettings.
            tofSpatialProfileCustomized();

    snapshot.spatialPersisted =
        runtimeSettings.
            tofSpatialProfilePersisted();

    snapshot.gainPoints =
        runtimeSettings.
            tofGainPoints();

    snapshot.gainPointCount =
        runtimeSettings.
            tofGainPointCount();

    snapshot.gainCustomized =
        runtimeSettings.
            tofGainCurveCustomized();

    snapshot.gainPersisted =
        runtimeSettings.
            tofGainCurvePersisted();

    snapshot.ledMapping =
        runtimeSettings.
            ledMappingProfile();

    snapshot.ledMappingCustomized =
        runtimeSettings.
            ledMappingProfileCustomized();

    snapshot.ledMappingPersisted =
        runtimeSettings.
            ledMappingProfilePersisted();

    snapshot.ledPixelMask =
        runtimeSettings.
            ledPixelMaskProfile();

    snapshot.ledPixelMaskCustomized =
        runtimeSettings.
            ledPixelMaskProfileCustomized();

    snapshot.ledPixelMaskPersisted =
        runtimeSettings.
            ledPixelMaskProfilePersisted();

    snapshot.commissioningPattern =
        static_cast<std::uint8_t>(
            commissioningPattern);

    const std::uint64_t nowUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    snapshot.commissioningRemainingMs =
        commissioningPattern !=
                ambilight::
                    LedCommissioningPattern::
                        None &&
            commissioningUntilUs >
                nowUs
            ? static_cast<std::uint32_t>(
                  (
                    commissioningUntilUs -
                    nowUs
                  ) /
                  1000ULL)
            : 0U;

    snapshot.calibrationActive =
        calibrationCapture.active();

    snapshot.calibrationSamples =
        calibrationCapture.
            storedSamples();

    snapshot.calibrationSummaryAvailable =
        haveLastCalibrationSummary;

    if (haveLastCalibrationSummary) {
        snapshot.calibrationSummary =
            lastCalibrationSummary;
    }

    snapshot.shadowProbeActive =
        shadowGainProbeActive();

    snapshot.lastActionSequence =
        lastWebActionSequence;

    snapshot.lastActionOk =
        lastWebActionOk;

    snapshot.lastActionMessage =
        lastWebActionMessage;

    snapshot.freeHeapBytes =
        ESP.getFreeHeap();

    snapshot.minFreeHeapBytes =
        ESP.getMinFreeHeap();

    return true;
}

void handleWebUiAction(
    ambilight::WebUiActionEvent event) {

    if (!event.ready()) {
        return;
    }

    const char* message =
        "ok";

    bool ok = false;

    switch (event.kind) {
    case ambilight::WebUiActionKind::
        Brightness: {

        std::uint8_t value = 0;

        const auto parsed =
            ambilight::
                RuntimePayloadParser::
                    parseBrightness(
                        event.text(),
                        value);

        ok =
            parsed ==
            ambilight::
                RuntimePayloadParseResult::
                    Ok;

        if (ok) {
            setOutputBrightness(
                value);

            message =
                "Brightness applied.";
        } else {
            message =
                "Invalid brightness. Use 0..255.";
        }

        break;
    }

    case ambilight::WebUiActionKind::
        Correction:

        if (std::strcmp(
                event.text(),
                "0") == 0) {

            setCorrectionMode(
                ambilight::
                    CorrectionMode::
                        Disabled);

            ok = true;
        } else if (
            std::strcmp(
                event.text(),
                "1") == 0) {

            setCorrectionMode(
                ambilight::
                    CorrectionMode::
                        Shadow);

            ok = true;
        } else if (
            std::strcmp(
                event.text(),
                "2") == 0) {

            setCorrectionMode(
                ambilight::
                    CorrectionMode::
                        Active);

            ok = true;
        }

        message =
            ok
                ? "Correction mode applied."
                : "Invalid correction mode.";

        break;

    case ambilight::WebUiActionKind::
        Commissioning: {

        const std::uint64_t nowUs =
            static_cast<std::uint64_t>(
                esp_timer_get_time());

        if (std::strcmp(
                event.text(),
                "0") == 0) {

            finishCommissioning(
                nowUs,
                true,
                "web cancel");

            ok = true;
            message =
                "LED test stopped.";
            break;
        }

        const std::uint8_t brightness =
            ledEngine.brightness();

        if (brightness == 0 ||
            brightness >
                kCommissioningMaxBrightness) {

            message =
                "LED test requires brightness 1..64.";
            break;
        }

        if (std::strcmp(
                event.text(),
                "1") == 0) {

            startCommissioning(
                ambilight::
                    LedCommissioningPattern::
                        SegmentIdentity);

            ok = true;
        } else if (
            std::strcmp(
                event.text(),
                "2") == 0) {

            startCommissioning(
                ambilight::
                    LedCommissioningPattern::
                        DirectionMarkers);

            ok = true;
        }

        if (!ok &&
            (std::strncmp(
                 event.text(),
                 "side:",
                 5) == 0 ||
             std::strncmp(
                 event.text(),
                 "gpio:",
                 5) == 0)) {

            ok =
                handleCommissioningRangePayload(
                    event.text());
        }

        message =
            ok
                ? "LED test started."
                : "Invalid/refused LED test.";

        break;
    }

    case ambilight::WebUiActionKind::
        LedMap:

        if (std::strcmp(
                event.text(),
                "reset") == 0) {

            ok =
                resetLedMappingProfile();

            message =
                ok
                    ? "LED mapping reset."
                    : "LED mapping reset refused.";
            break;
        }

        {
            ambilight::
                LedMappingProfile profile;

            const auto parsed =
                ambilight::
                    RuntimePayloadParser::
                        parseLedMapping(
                            event.text(),
                            profile);

            if (parsed !=
                ambilight::
                    RuntimePayloadParseResult::
                        Ok) {

                message =
                    "Invalid LED mapping.";
                break;
            }

            ok =
                applyLedMappingProfile(
                    profile);

            message =
                ok
                    ? "LED mapping applied."
                    : "LED mapping change refused.";
        }

        break;

    case ambilight::WebUiActionKind::
        PixelMask:

        if (std::strcmp(
                event.text(),
                "reset") == 0) {

            ok =
                resetLedPixelMaskProfile();

            message =
                ok
                    ? "Disabled-pixel mask reset."
                    : "Disabled-pixel mask reset failed.";
            break;
        }

        {
            ambilight::
                LedPixelMaskProfile profile;

            const auto parsed =
                ambilight::
                    RuntimePayloadParser::
                        parseLedPixelMask(
                            event.text(),
                            profile);

            if (parsed !=
                ambilight::
                    RuntimePayloadParseResult::
                        Ok) {

                message =
                    "Invalid disabled-pixel mask.";
                break;
            }

            ok =
                applyLedPixelMaskProfile(
                    profile);

            message =
                ok
                    ? "Disabled-pixel mask applied."
                    : "Disabled-pixel mask change failed.";
        }

        break;

    case ambilight::WebUiActionKind::
        Spatial:

        if (std::strcmp(
                event.text(),
                "reset") == 0) {

            ok =
                resetSpatialProfile();

            message =
                ok
                    ? "Spatial profile reset."
                    : "Spatial reset refused.";
            break;
        }

        {
            ambilight::
                TofSpatialProfile profile;

            const auto parsed =
                ambilight::
                    RuntimePayloadParser::
                        parseSpatialProfile(
                            event.text(),
                            profile);

            if (parsed !=
                ambilight::
                    RuntimePayloadParseResult::
                        Ok) {

                message =
                    "Invalid spatial profile.";
                break;
            }

            ok =
                applySpatialProfile(
                    profile);

            message =
                ok
                    ? "Spatial profile applied."
                    : "Spatial change refused.";
        }

        break;

    case ambilight::WebUiActionKind::
        GainCurve:

        if (std::strcmp(
                event.text(),
                "reset") == 0) {

            ok =
                resetTofGainCurve();

            message =
                ok
                    ? "Gain curve reset."
                    : "Gain curve reset refused.";
            break;
        }

        {
            std::array<
                ambilight::GainPoint,
                ambilight::
                    DistanceGainCurve::
                        kMaxPoints>
                points{};

            std::size_t count = 0;

            const auto parsed =
                ambilight::
                    RuntimePayloadParser::
                        parseGainCurve(
                            event.text(),
                            points,
                            count);

            if (parsed !=
                ambilight::
                    RuntimePayloadParseResult::
                        Ok) {

                message =
                    "Invalid gain curve.";
                break;
            }

            ok =
                applyTofGainCurve(
                    points,
                    count);

            message =
                ok
                    ? "Gain curve applied."
                    : "Gain curve change refused.";
        }

        break;

    case ambilight::WebUiActionKind::
        Wifi:

        if (std::strcmp(
                event.text(),
                "clear") == 0) {

            const bool cleared =
                clearRuntimeWifiCredentials();

            recordWebAction(
                event.sequence,
                cleared,
                cleared
                    ? "Wi-Fi NVS credentials cleared."
                    : "Wi-Fi runtime changed; NVS clear failed.");

            return;
        }

        {
            char* separator =
                std::strchr(
                    event.text(),
                    '|');

            if (separator == nullptr) {
                message =
                    "Wi-Fi payload must be SSID|PASSWORD.";
                break;
            }

            *separator = '\0';

            ok =
                applyWifiCredentials(
                    event.text(),
                    separator + 1,
                    true);

            if (!ok) {
                message =
                    "Wi-Fi credentials rejected.";
            } else if (
                wifiCredentialSource ==
                    WifiCredentialSource::Nvs) {

                message =
                    "Wi-Fi saved; reconnecting.";
            } else {
                message =
                    "Wi-Fi applied runtime-only; NVS write failed.";
            }
        }

        break;

    case ambilight::WebUiActionKind::
        Calibration:

        if (std::strcmp(
                event.text(),
                "start") != 0) {

            message =
                "Invalid calibration action.";
            break;
        }

        if (calibrationCapture.active()) {
            message =
                "Calibration capture is already active.";
            break;
        }

        startCalibrationCapture();
        ok = true;
        message =
            "60 s calibration capture started.";
        break;

    case ambilight::WebUiActionKind::
        ShadowProbe:

        if (std::strcmp(
                event.text(),
                "start") != 0) {

            message =
                "Invalid shadow probe action.";
            break;
        }

        if (correctionMode !=
            ambilight::
                CorrectionMode::
                    Shadow) {

            message =
                "Shadow probe requires SHADOW mode.";
            break;
        }

        startShadowGainProbe();
        ok = true;
        message =
            "10 s shadow probe started.";
        break;

    case ambilight::WebUiActionKind::
        TofDebug:

        if (std::strcmp(
                event.text(),
                "start") == 0) {

            ok =
                startTofDebug();

            message =
                ok
                    ? "60 s ToF live debug started."
                    : "ToF debug requires DISABLED or SHADOW.";
        } else if (
            std::strcmp(
                event.text(),
                "stop") == 0) {

            stopTofDebug(
                "web stop");

            ok = true;
            message =
                "ToF live debug stopped.";
        } else {
            message =
                "Invalid ToF debug action.";
        }

        break;

    case ambilight::WebUiActionKind::
        FactoryReset:

        if (std::strcmp(
                event.text(),
                "reset") != 0) {

            message =
                "Invalid factory reset action.";
            break;
        }

        if (ledEngine.brightness() != 0) {
            message =
                "Factory reset requires brightness 0.";
            break;
        }

        recordWebAction(
            event.sequence,
            true,
            "Factory reset accepted.");

        if (!handleFactoryCommand(
                "reset")) {

            recordWebAction(
                event.sequence,
                false,
                "Factory reset failed: NVS clear failed.");
        }

        return;

    case ambilight::WebUiActionKind::
        None:
        message =
            "Unknown web action.";
        break;
    }

    recordWebAction(
        event.sequence,
        ok,
        message);
}

void handleBrightnessCommand(
    const char* command) {

    if (command == nullptr ||
        command[0] == '\0') {

        printOutputBrightness();
        return;
    }

    std::uint8_t value = 0;

    const auto parseResult =
        ambilight::RuntimePayloadParser::
            parseBrightness(
                command,
                value);

    if (parseResult ==
        ambilight::RuntimePayloadParseResult::
            OutOfRange) {

        Serial.println(
            "OUTPUT brightness out of range. Use 0..255.");
        return;
    }

    if (parseResult !=
        ambilight::RuntimePayloadParseResult::Ok) {

        Serial.println(
            "OUTPUT brightness command invalid. Use b0..b255.");
        return;
    }

    setOutputBrightness(
        value);
}

void handleCorrectionCommand(
    const char* command) {

    if (command == nullptr ||
        command[0] == '\0') {

        // Preserve the historical ! + Enter no-op.
        return;
    }

    if (command[0] == '0' &&
        command[1] == '\0') {

        setCorrectionMode(
            ambilight::CorrectionMode::Disabled);
        return;
    }

    if (command[0] == '1' &&
        command[1] == '\0') {

        setCorrectionMode(
            ambilight::CorrectionMode::Shadow);
        return;
    }

    if (command[0] == '2' &&
        command[1] == '\0') {

        setCorrectionMode(
            ambilight::CorrectionMode::Active);
        return;
    }

    Serial.println(
        "CORRECTION command invalid. Use !0, !1 or !2.");
}

void handleCommissioningCommand(
    const char* command) {

    const std::uint64_t nowUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    if (command == nullptr ||
        command[0] == '\0') {

        printCommissioningStatus();
        return;
    }

    if (command[1] != '\0') {
        Serial.println(
            "LED TEST invalid. Use i0, i1, i2, or i + Enter.");
        return;
    }

    switch (command[0]) {
    case '0':
        finishCommissioning(
            nowUs,
            true,
            "serial cancel");
        return;

    case '1':
        startCommissioning(
            ambilight::LedCommissioningPattern::
                SegmentIdentity);
        return;

    case '2':
        startCommissioning(
            ambilight::LedCommissioningPattern::
                DirectionMarkers);
        return;

    default:
        Serial.println(
            "LED TEST invalid. Use i0, i1, i2, or i + Enter.");
        return;
    }
}

void printSerialFramingError(
    const ambilight::SerialCommandEvent& event) {

    const bool tooLong =
        event.error ==
        ambilight::SerialCommandError::TooLong;

    switch (event.kind) {
    case ambilight::SerialCommandKind::Factory:
        Serial.println(
            "FACTORY RESET command invalid/too long.");
        break;

    case ambilight::SerialCommandKind::CommissioningRange:
        Serial.println(
            "LED RANGE command invalid/too long.");
        break;

    case ambilight::SerialCommandKind::LedMap:
        Serial.println(
            "LED TOPOLOGY command invalid/too long.");
        break;

    case ambilight::SerialCommandKind::LedPixelMask:
        Serial.println(
            "LED PIXEL MASK command invalid/too long.");
        break;

    case ambilight::SerialCommandKind::Spatial:
        Serial.println(
            tooLong
                ? "TOF SPATIAL command too long."
                : "TOF SPATIAL command contains unsupported control characters.");
        break;

    case ambilight::SerialCommandKind::GainCurve:
        Serial.println(
            tooLong
                ? "TOF CURVE command too long."
                : "TOF CURVE command contains unsupported control characters.");
        break;

    case ambilight::SerialCommandKind::Wifi:
        Serial.println(
            tooLong
                ? "Wi-Fi command too long."
                : "Wi-Fi command contains unsupported control characters.");
        break;

    case ambilight::SerialCommandKind::Brightness:
        Serial.println(
            "OUTPUT brightness command invalid. Use b0..b255.");
        break;

    default:
        Serial.println(
            "SERIAL command framing error.");
        break;
    }
}

void dispatchSerialCommand(
    ambilight::SerialCommandEvent event) {

    if (!event.valid()) {
        printSerialFramingError(
            event);
        return;
    }

    switch (event.kind) {
    case ambilight::SerialCommandKind::Correction:
        handleCorrectionCommand(
            event.text());
        break;

    case ambilight::SerialCommandKind::Commissioning:
        handleCommissioningCommand(
            event.text());
        break;

    case ambilight::SerialCommandKind::Factory:
        handleFactoryCommand(
            event.text());
        break;

    case ambilight::SerialCommandKind::CommissioningRange:
        handleCommissioningRangePayload(
            event.text());
        break;

    case ambilight::SerialCommandKind::LedMap:
        handleLedMapCommand(
            event.text());
        break;

    case ambilight::SerialCommandKind::LedPixelMask:
        handleLedPixelMaskCommand(
            event.text());
        break;

    case ambilight::SerialCommandKind::Spatial:
        handleSpatialCommand(
            event.text());
        break;

    case ambilight::SerialCommandKind::GainCurve:
        handleGainCurveCommand(
            event.text());
        break;

    case ambilight::SerialCommandKind::Wifi:
        handleWifiCommand(
            event.text());
        break;

    case ambilight::SerialCommandKind::Brightness:
        handleBrightnessCommand(
            event.text());
        break;

    case ambilight::SerialCommandKind::DumpTofRaw:
        dumpTofMap();
        break;

    case ambilight::SerialCommandKind::DumpTofGeometry:
        dumpTofGeometry();
        break;

    case ambilight::SerialCommandKind::DumpTofPlane:
        dumpTofPlane();
        break;

    case ambilight::SerialCommandKind::DumpTofGains:
        dumpTofGains();
        break;

    case ambilight::SerialCommandKind::DumpSpatialGains:
        dumpSpatialGains();
        break;

    case ambilight::SerialCommandKind::StartCalibrationCapture:
        startCalibrationCapture();
        break;

    case ambilight::SerialCommandKind::DumpRender:
        dumpRenderShadow();
        break;

    case ambilight::SerialCommandKind::StartShadowProbe:
        startShadowGainProbe();
        break;

    case ambilight::SerialCommandKind::TofDebugToggle:
        toggleTofDebug();
        break;

    case ambilight::SerialCommandKind::CorrectionStatus:
        printCorrectionMode();
        break;

    case ambilight::SerialCommandKind::FirmwareStatus:
        printFirmwareInfo();
        break;

    case ambilight::SerialCommandKind::None:
        break;
    }
}

void serviceDebugCommands() {
    while (Serial.available() > 0) {
        auto event =
            serialCommandParser.feed(
                Serial.read());

        if (!event.ready()) {
            continue;
        }

        dispatchSerialCommand(
            event);
    }
}

void printRuntimeStatus() {
    char senderIp[INET_ADDRSTRLEN] = "none";

    const std::uint32_t sender =
        ddp.activeSenderIpv4NetworkOrder();

    if (sender != 0) {
        in_addr address{};
        address.s_addr = sender;
        inet_ntop(AF_INET, &address, senderIp, sizeof(senderIp));
    }

    const auto& udp = ddp.stats();
    const auto& asmStats = ddp.assemblerStats();
    const auto& senderStats =
        ddp.senderGateStats();

    ambilight::TofSnapshot tofSnapshot;
    const bool haveTof = tof.copySnapshot(tofSnapshot);

    const std::uint64_t nowUs =
        static_cast<std::uint64_t>(esp_timer_get_time());

    const std::uint64_t tofAgeMs =
        haveTof && tofSnapshot.timestampUs != 0
            ? (nowUs - tofSnapshot.timestampUs) / 1000ULL
            : 0;

    const auto& geometry = tofSnapshot.geometry;

    Serial.printf(
        "STAT corr=%s brightness=%u persist=%s wifi=%s wsrc=%s rssi=%d pkt=%lu asm=%lu pub=%lu collapse=%lu rej=%lu stale=%lu timeout=%lu "
        "budget=%lu lim=%lu pollmax=%luus rxreq=%ld rxactual=%ld rxset=%s rxget=%s optwarn=%lu "
        "sender_lock=%s sender=%s:%u "
        "saccept=%lu sinvalid=%lu sforeign=%lu sacq=%lu srel=%lu render=%lu skip=%lu "
        "p50<=%luus p95<=%luus p99<=%luus ovf=%llu agemax=%lluus showmax=%luus "
        "tof=%s tofgen=%lu rawvalid=%u rawmed=%u tofage=%llums tofread=%luus tofreadmax=%luus "
        "geom=%s l=%u c=%u r=%u delta=%d acc=%u "
        "plane=%s pyaw=%d ppitch=%d pacc=%u pmad=%u "
        "pcalc=%lu pskip=%lu pdelta=%u pfail=%lu "
        "spfail=%s spmin=%u spmax=%u "
        "gainfail=%s gl=%u gt=%u gb=%u gr=%u "
        "shadow_usable=%lu shadow_nonunity=%lu shadow_changed=%u phys_changed=%u shadow_delta=%u shadowprep=%luus "
        "slew_snap=%lu probe=%s sched_rgb=%lu sched_state=%lu sched_comb=%lu sched_state_def=%lu rgbgen=%lu "
        "tofinit=%lu toffail=%lu tofreadfail=%lu tofrestart=%lu black=%lu heap=%lu minheap=%lu\n",
        ambilight::correctionModeName(
            correctionMode),
        static_cast<unsigned>(
            ledEngine.brightness()),
        runtimeSettings.persistenceAvailable()
            ? "yes"
            : "no",
        wifi.connected() ? "up" : "down",
        wifiCredentialSourceName(
            wifiCredentialSource),
        wifi.connected() ? WiFi.RSSI() : 0,
        static_cast<unsigned long>(udp.datagramsReceived),
        static_cast<unsigned long>(udp.completeFramesAssembled),
        static_cast<unsigned long>(udp.mailboxPublications),
        static_cast<unsigned long>(udp.collapsedCompleteFrames),
        static_cast<unsigned long>(asmStats.rejected),
        static_cast<unsigned long>(asmStats.stale),
        static_cast<unsigned long>(asmStats.timedOut),
        static_cast<unsigned long>(udp.pollBudgetExhaustions),
        static_cast<unsigned long>(udp.pollDatagramLimitHits),
        static_cast<unsigned long>(udp.maxPollUs),
        static_cast<long>(udp.requestedRxBufferBytes),
        static_cast<long>(udp.actualRxBufferBytes),
        udp.rxBufferSetOk ? "ok" : "no",
        udp.rxBufferQueryOk ? "ok" : "no",
        static_cast<unsigned long>(
            udp.socketOptionWarnings),
        ddp.senderLocked() ? "yes" : "no",
        senderIp,
        ddp.activeSenderPort(),
        static_cast<unsigned long>(
            senderStats.acceptedDatagrams),
        static_cast<unsigned long>(
            senderStats.invalidDatagrams),
        static_cast<unsigned long>(
            senderStats.foreignSenderDrops),
        static_cast<unsigned long>(
            senderStats.lockAcquisitions),
        static_cast<unsigned long>(
            senderStats.lockTimeoutReleases),
        static_cast<unsigned long>(renderer.renderedFrames()),
        static_cast<unsigned long>(backlogRenderSkips),
        static_cast<unsigned long>(
            frameAgeHistogram.percentileUpperBoundUs(50)),
        static_cast<unsigned long>(
            frameAgeHistogram.percentileUpperBoundUs(95)),
        static_cast<unsigned long>(
            frameAgeHistogram.percentileUpperBoundUs(99)),
        static_cast<unsigned long long>(frameAgeHistogram.overflow()),
        static_cast<unsigned long long>(maxFrameAgeUs),
        static_cast<unsigned long>(ledEngine.maxShowTimeUs()),
        haveTof ? tofStateName(tofSnapshot.state) : "unavailable",
        haveTof
            ? static_cast<unsigned long>(tofSnapshot.generation)
            : 0UL,
        haveTof
            ? static_cast<unsigned>(tofSnapshot.validZones)
            : 0U,
        haveTof
            ? tofSnapshot.medianMm
            : 0U,
        static_cast<unsigned long long>(tofAgeMs),
        haveTof
            ? static_cast<unsigned long>(tofSnapshot.lastReadUs)
            : 0UL,
        haveTof
            ? static_cast<unsigned long>(tofSnapshot.maxReadUs)
            : 0UL,
        haveTof && geometry.valid ? "ok" : "bad",
        haveTof && geometry.left.valid
            ? geometry.left.filteredMm
            : 0U,
        haveTof && geometry.center.valid
            ? geometry.center.filteredMm
            : 0U,
        haveTof && geometry.right.valid
            ? geometry.right.filteredMm
            : 0U,
        haveTof
            ? static_cast<int>(geometry.rightMinusLeftMm)
            : 0,
        haveTof
            ? static_cast<unsigned>(geometry.acceptedZones)
            : 0U,
        haveTof && geometry.plane.valid ? "ok" : "bad",
        haveTof
            ? static_cast<int>(geometry.plane.yawCentiDeg)
            : 0,
        haveTof
            ? static_cast<int>(geometry.plane.pitchCentiDeg)
            : 0,
        haveTof
            ? static_cast<unsigned>(geometry.plane.accepted)
            : 0U,
        haveTof
            ? geometry.plane.residualMadMm
            : 0U,
        haveTof
            ? static_cast<unsigned long>(
                  tofSnapshot.planeRecalculations)
            : 0UL,
        haveTof
            ? static_cast<unsigned long>(
                  tofSnapshot.planeDeadbandSkips)
            : 0UL,
        haveTof
            ? tofSnapshot.lastPlaneWallDeltaMm
            : 0U,
        haveTof
            ? static_cast<unsigned long>(
                  tofSnapshot.planeFailOpens)
            : 0UL,
        haveTof && tofSnapshot.perimeterGains.failOpen ? "yes" : "no",
        haveTof
            ? tofSnapshot.perimeterGains.minDistanceMm
            : 0U,
        haveTof
            ? tofSnapshot.perimeterGains.maxDistanceMm
            : 0U,
        haveTof && tofSnapshot.gains.failOpen ? "yes" : "no",
        haveTof ? tofSnapshot.gains.leftQ12 : ambilight::kGainUnityQ12,
        haveTof ? tofSnapshot.gains.topQ12 : ambilight::kGainUnityQ12,
        haveTof ? tofSnapshot.gains.bottomQ12 : ambilight::kGainUnityQ12,
        haveTof ? tofSnapshot.gains.rightQ12 : ambilight::kGainUnityQ12,
        static_cast<unsigned long>(
            renderer.shadowStats().sourceUsableFrames),
        static_cast<unsigned long>(
            renderer.shadowStats().nonUnityContextFrames),
        renderer.shadowStats().lastWouldChangePixels,
        renderer.shadowStats().lastPhysicalChangedPixels,
        static_cast<unsigned>(
            renderer.shadowStats().lastMaxChannelDelta),
        static_cast<unsigned long>(
            renderer.shadowStats().lastPrepareUs),
        static_cast<unsigned long>(
            renderGainController.stats().failOpenUnitySnaps),
        shadowGainProbeActive() ? "yes" : "no",
        static_cast<unsigned long>(
            renderScheduler.stats().rgbRenders),
        static_cast<unsigned long>(
            renderScheduler.stats().stateOnlyRenders),
        static_cast<unsigned long>(
            renderScheduler.stats().combinedRenders),
        static_cast<unsigned long>(
            renderScheduler.stats().stateDeferrals),
        static_cast<unsigned long>(
            lastRenderedRgbGeneration),
        haveTof
            ? static_cast<unsigned long>(tofSnapshot.initAttempts)
            : 0UL,
        haveTof
            ? static_cast<unsigned long>(tofSnapshot.initFailures)
            : 0UL,
        haveTof
            ? static_cast<unsigned long>(
                  tofSnapshot.rangingReadFailures)
            : 0UL,
        haveTof
            ? static_cast<unsigned long>(tofSnapshot.staleRestarts)
            : 0UL,
        static_cast<unsigned long>(idleBlackouts),
        static_cast<unsigned long>(ESP.getFreeHeap()),
        static_cast<unsigned long>(ESP.getMinFreeHeap()));

    Serial.printf(
        "STATCFG fw=%s stage=%u curve=%s curve_points=%u curve_updates=%lu spatial=%s spatial_updates=%lu ledmap=%s topo_leds=%u ddp_bytes=%u pixelmask=%s ledtest=%s tofdebug=%s\n",
        ambilight::config::kFirmwareVersion,
        static_cast<unsigned>(
            ambilight::config::kDevelopmentStage),
        gainCurveSourceName(),
        static_cast<unsigned>(
            runtimeSettings.tofGainPointCount()),
        haveTof
            ? static_cast<unsigned long>(
                  tofSnapshot.gainCurveUpdates)
            : 0UL,
        spatialProfileSourceName(),
        haveTof
            ? static_cast<unsigned long>(
                  tofSnapshot.spatialProfileUpdates)
            : 0UL,
        ledMappingSourceName(),
        static_cast<unsigned>(
            runtimeSettings.
                ledMappingProfile().
                totalLedCount()),
        static_cast<unsigned>(
            runtimeSettings.
                ledMappingProfile().
                totalLedCount() *
            sizeof(ambilight::Rgb8)),
        ledPixelMaskSourceName(),
        commissioningPatternName(
            commissioningPattern),
        tofDebugActive()
            ? "yes"
            : "no");
}

void printConfiguration() {
    Serial.println();
    Serial.printf(
        "%s %s Stage %u target=%s serial_proto=%u\n",
        ambilight::config::kFirmwareName,
        ambilight::config::kFirmwareVersion,
        static_cast<unsigned>(
            ambilight::config::kDevelopmentStage),
        ambilight::config::kFirmwareTarget,
        static_cast<unsigned>(
            ambilight::config::kSerialProtocolVersion));

    Serial.printf(
        "Logical LEDs=%u/%u capacity payload=%uB DDP=%u poll_budget=%luus max_datagrams=%u\n",
        static_cast<unsigned>(
            runtimeSettings.
                ledMappingProfile().
                totalLedCount()),
        static_cast<unsigned>(
            ambilight::config::
                kLogicalLedCapacity),
        static_cast<unsigned>(
            runtimeSettings.
                ledMappingProfile().
                totalLedCount() *
            sizeof(ambilight::Rgb8)),
        ambilight::DdpUdpService::kPort,
        static_cast<unsigned long>(
            ambilight::DdpUdpService::kPollBudgetUs),
        static_cast<unsigned>(
            ambilight::DdpUdpService::kMaxDatagramsPerPoll));

    Serial.printf(
        "PARLIO lanes=%u lane_length=%u brightness=%u/255\n",
        static_cast<unsigned>(ambilight::config::kParlioLaneCount),
        static_cast<unsigned>(ambilight::config::kPhysicalLaneLength),
        static_cast<unsigned>(
            ledEngine.brightness()));

    Serial.printf(
        "Web UI HTTP/%u single-client, bounded 512B recv / 1024B send per main-loop service step.\n",
        static_cast<unsigned>(
            ambilight::WebUiService::kPort));

    const auto& spatialProfile =
        runtimeSettings.tofSpatialProfile();

    Serial.printf(
        "VL53L5CX SDA=%u SCL=%u 8x8 internal 1Hz, pose processed about every 12s, "
        "spatial=%s %.1fx%.1fmm sensor=(%.1f,%.1f) led_z=%.1fmm "
        "plane_deadband=%.1fmm rotation=%u mirror_x=%s; "
        "physical correction follows the persisted runtime mode.\n",
        ambilight::config::kTofSdaGpio,
        ambilight::config::kTofSclGpio,
        spatialProfileSourceName(),
        static_cast<double>(
            spatialProfile.widthMm()),
        static_cast<double>(
            spatialProfile.heightMm()),
        static_cast<double>(
            spatialProfile.sensorOffsetXmm()),
        static_cast<double>(
            spatialProfile.sensorOffsetYmm()),
        static_cast<double>(
            spatialProfile.ledPlaneZmm()),
        static_cast<double>(
            spatialProfile.planeDeadbandMm()),
        static_cast<unsigned>(
            spatialProfile.rotationQuarterTurns),
        spatialProfile.mirrorX ? "yes" : "no");

    Serial.printf(
        "Correction mode=%s (NVS=%s). !0=DISABLED, !1=SHADOW, !2=ACTIVE, m=status.\n",
        ambilight::correctionModeName(
            correctionMode),
        runtimeSettings.persistenceAvailable()
            ? "available"
            : "unavailable");

    Serial.printf(
        "ToF gain curve=%s points=%u; q=status/set/reset.\n",
        gainCurveSourceName(),
        static_cast<unsigned>(
            runtimeSettings.tofGainPointCount()));

    Serial.println(
        "Debug: 't'=raw, 'g'=bands, 'p'=plane, 'k'=legacy gains, 's'=spatial gains, 'c'=capture, 'r'=render, 'x'=shadow probe, 'z'=ToF live debug toggle, 'v'=firmware.");
    Serial.println(
        "Output brightness: b0..b255 followed by Enter; b + Enter prints status.");
    Serial.println(
        "Wi-Fi: wSSID|PASSWORD + Enter sets/reconnects, w + Enter=status, wclear + Enter=clear NVS.");
    Serial.println(
        "ToF curve: q + Enter=status, qreset + Enter=default, qDIST:GAIN,... + Enter=set (not in ACTIVE).");
    Serial.println(
        "Spatial: y + Enter=status, yreset, or yW,H,X,Y,Z,ROT,MIRROR,DEADBAND (mm, not in ACTIVE).");
    Serial.println(
        "LED topology: l + Enter=status, lreset, or lCOUNT:GPIO:REV,... Example l230:18:0,160:19:0,230:20:0,160:21:0; brightness must be 0.");
    Serial.println(
        "LED pixel mask: d + Enter=status, dreset, or dTOP,RIGHT,BOTTOM,LEFT; '-' means none. Example: d-,12,-,0.");
    Serial.println(
        "LED test: i1=segments, i2=direction, i0=stop; jside:SIDE:START:COUNT or jgpio:GPIO:START:COUNT; brightness 1..64.");
    Serial.println(
        "Factory recovery: freset + Enter clears ambilight NVS and restarts; brightness must be 0.");

    printLedMappingProfile();
    printLedPixelMaskProfile();

    Serial.println(
        "Active frame transport remains Wi-Fi/DDP only. "
        "USB/AWA is preserved separately as WIP.");
    Serial.println();
}

} // namespace

void setup() {
    Serial.begin(115200);
    delay(1000);

    const esp_reset_reason_t resetReason =
        esp_reset_reason();

    Serial.printf(
        "BOOT DIAG reset_reason=%d (%s)\n",
        static_cast<int>(resetReason),
        resetReasonName(resetReason));

    printLedRuntimeProbe(
        "setup-entry");

    if (!runtimeSettings.begin()) {
        Serial.println(
            "Runtime settings warning: NVS unavailable; using volatile defaults.");
    }

    correctionMode =
        runtimeSettings.correctionMode();

    ledEngine.setBrightness(
        runtimeSettings.outputBrightness());

    const auto& startupTopology =
        runtimeSettings.ledMappingProfile();

    if (!renderer.setMappingProfile(
            startupTopology)) {

        fatal(
            "LED runtime topology profile is invalid",
            ESP_ERR_INVALID_ARG);
    }

    if (!ddp.setLogicalLedCount(
            startupTopology.totalLedCount())) {

        fatal(
            "DDP runtime topology size is invalid",
            ESP_ERR_INVALID_ARG);
    }

    if (!tof.setLedTopology(
            startupTopology)) {

        fatal(
            "ToF runtime topology profile is invalid",
            ESP_ERR_INVALID_ARG);
    }

    cachedTargetGainContext =
        ambilight::RenderGainContext::unity(
            startupTopology);

    renderSnapshot.pixelCount =
        startupTopology.totalLedCount();

    commissioningFrame.pixelCount =
        startupTopology.totalLedCount();

    if (!renderer.setPixelMaskProfile(
            runtimeSettings.ledPixelMaskProfile())) {

        fatal(
            "LED runtime pixel-mask profile is invalid",
            ESP_ERR_INVALID_ARG);
    }

    if (!tof.setSpatialProfile(
            runtimeSettings.tofSpatialProfile())) {

        fatal(
            "ToF runtime spatial profile is invalid",
            ESP_ERR_INVALID_ARG);
    }

    if (!tof.setGainCurve(
            runtimeSettings.tofGainCurve())) {

        fatal(
            "ToF runtime gain curve is invalid",
            ESP_ERR_INVALID_ARG);
    }

    printConfiguration();

    if (!mailbox.begin()) {
        fatal("FrameMailbox::begin failed", ESP_ERR_NO_MEM);
    }

    const esp_err_t ledResult = ledEngine.begin();
    if (ledResult != ESP_OK) {
        fatal("LedEngine::begin failed", ledResult);
    }

    const char* startupWifiSsid = "";
    const char* startupWifiPassword = "";

    if (runtimeSettings.wifiCredentialsPresent()) {
        startupWifiSsid =
            runtimeSettings.wifiSsid();

        startupWifiPassword =
            runtimeSettings.wifiPassword();

        wifiCredentialSource =
            WifiCredentialSource::Nvs;
    } else if (
        ambilight::config::wifiCredentialsPresent()) {

        startupWifiSsid =
            ambilight::config::kWifiSsid;

        startupWifiPassword =
            ambilight::config::kWifiPassword;

        wifiCredentialSource =
            WifiCredentialSource::CompileTime;
    } else {
        wifiCredentialSource =
            WifiCredentialSource::None;
    }

    if (!wifi.begin(
            startupWifiSsid,
            startupWifiPassword)) {

        fatal(
            "WifiService::begin failed",
            ESP_FAIL);
    }

    if (wifi.enabled()) {
        if (!ensureDdpRunning()) {
            fatal(
                "DdpUdpService::begin failed",
                ESP_FAIL);
        }

        ensureWebUiRunning();
    } else {
        Serial.println(
            "DDP/Web runtime inactive because Wi-Fi credentials are absent. Provision with wSSID|PASSWORD.");
    }

    if (!tof.begin()) {
        Serial.println(
            "VL53L5CX task creation failed; Ambilight continues without ToF.");
    } else {
        Serial.println(
            "VL53L5CX background initialization started; DDP remains available during sensor firmware upload.");
    }
}

void loop() {
    wifi.tick(millis());
    serviceDebugCommands();
    serviceCalibrationCapture();

    ambilight::DdpPollResult pollResult;
    if (ddp.running()) {
        pollResult = ddp.poll();
        updateDdpActivityState();
    }

    if (commissioningPattern ==
            ambilight::LedCommissioningPattern::None &&
        pollResult.backlogLikely &&
        consecutiveBacklogRenderSkips <
            kMaxConsecutiveBacklogRenderSkips) {

        ++consecutiveBacklogRenderSkips;
        ++backlogRenderSkips;

        delay(0);
        return;
    }

    consecutiveBacklogRenderSkips = 0;

    const std::uint64_t nowUs =
        static_cast<std::uint64_t>(esp_timer_get_time());

    serviceTofDebug(
        nowUs);

    if (webUi.running()) {
        auto webAction =
            webUi.poll(
                fillWebUiSnapshot,
                nowUs);

        if (webAction.ready()) {
            handleWebUiAction(
                webAction);
        }
    }

    serviceIdleBlackout(nowUs);

    if (!serviceCommissioning(
            nowUs)) {

        serviceRender(nowUs);
    }

    const std::uint32_t nowMs = millis();
    if (static_cast<std::int32_t>(
            nowMs - (lastStatusMs + kStatusIntervalMs)) >= 0) {
        printRuntimeStatus();
        lastStatusMs = nowMs;
    }

    delay(1);
}
