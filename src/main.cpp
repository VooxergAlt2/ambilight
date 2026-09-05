#include <Arduino.h>
#include <WiFi.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <esp_err.h>
#include <esp_timer.h>
#include <lwip/inet.h>

#include "config/BoardConfig.h"
#include "config/RuntimeSettings.h"
#include "config/WifiCredentials.h"
#include "core/FrameMailbox.h"
#include "core/LatencyHistogram.h"
#include "core/RgbFrame.h"
#include "led/LedEngine.h"
#include "led/LedRenderer.h"
#include "integration/TofRenderGainBridge.h"
#include "render/CorrectionMode.h"
#include "render/RenderGainController.h"
#include "render/ShadowGainProbe.h"
#include "render/RenderScheduler.h"
#include "network/DdpUdpService.h"
#include "network/WifiService.h"
#include "tof/TofCalibrationCapture.h"
#include "tof/TofService.h"

namespace {

constexpr std::uint64_t kIdleBlackoutUs = 1000000;
constexpr std::uint32_t kStatusIntervalMs = 30000;
constexpr std::uint8_t kMaxConsecutiveBacklogRenderSkips = 4;
constexpr std::uint64_t kGainTargetPollIntervalUs = 1000000;

constexpr std::size_t kWifiCommandBufferSize =
    ambilight::RuntimeSettings::kMaxWifiSsidLength +
    1 +
    ambilight::RuntimeSettings::kMaxWifiPasswordLength +
    1;

constexpr std::size_t kGainCurveCommandBufferSize = 128;
constexpr std::size_t kSpatialCommandBufferSize = 128;

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

ambilight::CorrectionMode correctionMode =
    ambilight::CorrectionMode::Shadow;

ambilight::RgbFrame renderSnapshot;
ambilight::PerimeterGainSnapshot cachedPerimeterGainSnapshot{};
ambilight::RenderGainContext cachedTargetGainContext{};

bool rgbFrameValid = false;
bool rgbDirty = false;
bool correctionModeDirty = false;
bool brightnessDirty = false;

bool correctionCommandPending = false;
bool brightnessCommandPending = false;
bool wifiCommandPending = false;
bool gainCurveCommandPending = false;
bool spatialCommandPending = false;

std::uint16_t brightnessCommandValue = 0;
std::uint8_t brightnessCommandDigits = 0;

std::array<char, kWifiCommandBufferSize>
    wifiCommandBuffer{};
std::size_t wifiCommandLength = 0;

std::array<char, kGainCurveCommandBufferSize>
    gainCurveCommandBuffer{};
std::size_t gainCurveCommandLength = 0;

std::array<char, kSpatialCommandBufferSize>
    spatialCommandBuffer{};
std::size_t spatialCommandLength = 0;

enum class WifiCredentialSource : std::uint8_t {
    None = 0,
    CompileTime,
    Nvs,
    RuntimeVolatile
};

WifiCredentialSource wifiCredentialSource =
    WifiCredentialSource::None;

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
std::uint64_t nextGainTargetPollUs = 0;

std::uint8_t consecutiveBacklogRenderSkips = 0;

bool idleBlanked = false;

std::uint64_t lastFrameAgeUs = 0;
std::uint64_t maxFrameAgeUs = 0;

bool shadowGainProbeActive();

bool parseUint16Token(
    const char*& cursor,
    std::uint16_t& value);

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
        "DDP socket bound to UDP/%u. RXBUF requested=%d actual=%d set=%s query=%s option_warnings=%lu.\n",
        ambilight::DdpUdpService::kPort,
        socketStats.requestedRxBufferBytes,
        socketStats.actualRxBufferBytes,
        socketStats.rxBufferSetOk ? "ok" : "no",
        socketStats.rxBufferQueryOk ? "ok" : "no",
        static_cast<unsigned long>(
            socketStats.socketOptionWarnings));

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

    Serial.printf(
        "Wi-Fi credentials applied: ssid='%s' source=%s password=hidden\n",
        wifi.ssid(),
        wifiCredentialSourceName(
            wifiCredentialSource));

    return true;
}

void clearRuntimeWifiCredentials() {
    const bool cleared =
        runtimeSettings.clearWifiCredentials();

    if (ambilight::config::wifiCredentialsPresent()) {
        if (!wifi.configure(
                ambilight::config::kWifiSsid,
                ambilight::config::kWifiPassword)) {

            wifi.disable();
            ddp.stop();

            wifiCredentialSource =
                WifiCredentialSource::None;

            Serial.println(
                "Compile-time Wi-Fi fallback is invalid; Wi-Fi/DDP disabled.");
            return;
        }

        wifiCredentialSource =
            WifiCredentialSource::CompileTime;

        ensureDdpRunning();

        Serial.printf(
            "Wi-Fi NVS credentials cleared (%s); using compile-time fallback SSID '%s'.\n",
            cleared ? "persisted" : "volatile-only",
            wifi.ssid());

        return;
    }

    wifi.disable();
    ddp.stop();

    wifiCredentialSource =
        WifiCredentialSource::None;

    Serial.printf(
        "Wi-Fi NVS credentials cleared (%s); no compile-time fallback, Wi-Fi/DDP disabled.\n",
        cleared ? "persisted" : "volatile-only");
}

void resetWifiCommand() {
    wifiCommandPending = false;
    wifiCommandLength = 0;
    wifiCommandBuffer.fill('\0');
}

void finishWifiCommand() {
    wifiCommandBuffer[
        wifiCommandLength] = '\0';

    if (wifiCommandLength == 0) {
        printWifiProvisioningStatus();
        resetWifiCommand();
        return;
    }

    if (std::strcmp(
            wifiCommandBuffer.data(),
            "clear") == 0) {

        clearRuntimeWifiCredentials();
        resetWifiCommand();
        return;
    }

    char* separator =
        std::strchr(
            wifiCommandBuffer.data(),
            '|');

    if (separator == nullptr) {
        Serial.println(
            "Wi-Fi command invalid. Use wSSID|PASSWORD or wclear.");
        resetWifiCommand();
        return;
    }

    *separator = '\0';

    const char* ssid =
        wifiCommandBuffer.data();

    const char* password =
        separator + 1;

    applyWifiCredentials(
        ssid,
        password,
        true);

    resetWifiCommand();
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

bool parseDecimalX10(
    const char*& cursor,
    std::int32_t& valueX10) {

    if (cursor == nullptr) {
        return false;
    }

    bool negative = false;

    if (*cursor == '-') {
        negative = true;
        ++cursor;
    }

    if (*cursor < '0' ||
        *cursor > '9') {
        return false;
    }

    std::int32_t whole = 0;

    while (*cursor >= '0' &&
           *cursor <= '9') {

        whole =
            whole * 10 +
            static_cast<std::int32_t>(
                *cursor - '0');

        if (whole > 100000) {
            return false;
        }

        ++cursor;
    }

    std::int32_t fraction = 0;

    if (*cursor == '.') {
        ++cursor;

        if (*cursor < '0' ||
            *cursor > '9') {
            return false;
        }

        fraction =
            static_cast<std::int32_t>(
                *cursor - '0');

        ++cursor;

        // Fixed profile precision is exactly 0.1 mm.
        if (*cursor >= '0' &&
            *cursor <= '9') {
            return false;
        }
    }

    std::int32_t result =
        whole * 10 +
        fraction;

    if (negative) {
        result = -result;
    }

    valueX10 = result;
    return true;
}

bool consumeComma(
    const char*& cursor) {

    if (*cursor != ',') {
        return false;
    }

    ++cursor;
    return true;
}

bool parseSpatialProfileText(
    const char* text,
    ambilight::TofSpatialProfile& profile) {

    if (text == nullptr ||
        *text == '\0') {
        return false;
    }

    const char* cursor = text;

    std::int32_t width = 0;
    std::int32_t height = 0;
    std::int32_t sensorX = 0;
    std::int32_t sensorY = 0;
    std::int32_t ledZ = 0;
    std::int32_t deadband = 0;

    std::uint16_t rotation = 0;
    std::uint16_t mirror = 0;

    if (!parseDecimalX10(cursor, width) ||
        !consumeComma(cursor) ||
        !parseDecimalX10(cursor, height) ||
        !consumeComma(cursor) ||
        !parseDecimalX10(cursor, sensorX) ||
        !consumeComma(cursor) ||
        !parseDecimalX10(cursor, sensorY) ||
        !consumeComma(cursor) ||
        !parseDecimalX10(cursor, ledZ) ||
        !consumeComma(cursor) ||
        !parseUint16Token(cursor, rotation) ||
        !consumeComma(cursor) ||
        !parseUint16Token(cursor, mirror) ||
        !consumeComma(cursor) ||
        !parseDecimalX10(cursor, deadband) ||
        *cursor != '\0') {

        return false;
    }

    if (width < 0 ||
        width > 65535 ||
        height < 0 ||
        height > 65535 ||
        sensorX < -32768 ||
        sensorX > 32767 ||
        sensorY < -32768 ||
        sensorY > 32767 ||
        ledZ < -32768 ||
        ledZ > 32767 ||
        deadband < 0 ||
        deadband > 65535 ||
        rotation > 255 ||
        mirror > 255) {

        return false;
    }

    profile.widthMmX10 =
        static_cast<std::uint16_t>(
            width);

    profile.heightMmX10 =
        static_cast<std::uint16_t>(
            height);

    profile.sensorOffsetXmmX10 =
        static_cast<std::int16_t>(
            sensorX);

    profile.sensorOffsetYmmX10 =
        static_cast<std::int16_t>(
            sensorY);

    profile.ledPlaneZmmX10 =
        static_cast<std::int16_t>(
            ledZ);

    profile.rotationQuarterTurns =
        static_cast<std::uint8_t>(
            rotation);

    profile.mirrorX =
        static_cast<std::uint8_t>(
            mirror);

    profile.planeDeadbandMmX10 =
        static_cast<std::uint16_t>(
            deadband);

    return profile.valid();
}

void invalidateRenderedGainAfterSpatialChange() {
    cachedPerimeterGainSnapshot = {};
    haveCachedPerimeterGainSnapshot = false;

    cachedTargetGainContext =
        ambilight::RenderGainContext::unity();

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

void resetSpatialProfile() {
    if (correctionMode ==
        ambilight::CorrectionMode::Active) {

        Serial.println(
            "TOF SPATIAL reset refused in ACTIVE mode. Switch to SHADOW or DISABLED first.");
        return;
    }

    const bool persisted =
        runtimeSettings.resetTofSpatialProfile();

    const auto profile =
        runtimeSettings.tofSpatialProfile();

    if (!tof.setSpatialProfile(
            profile)) {

        Serial.println(
            "TOF SPATIAL reset stored, but sensor service could not queue it. Reboot will load the default.");
        return;
    }

    invalidateRenderedGainAfterSpatialChange();

    Serial.printf(
        "TOF SPATIAL profile reset to default; persisted=%s. Waiting for next valid pose rebuild.\n",
        persisted ? "yes" : "no");

    printSpatialProfile();
}

void resetSpatialCommand() {
    spatialCommandPending = false;
    spatialCommandLength = 0;
    spatialCommandBuffer.fill('\0');
}

void finishSpatialCommand() {
    spatialCommandBuffer[
        spatialCommandLength] = '\0';

    if (spatialCommandLength == 0) {
        printSpatialProfile();
        resetSpatialCommand();
        return;
    }

    if (std::strcmp(
            spatialCommandBuffer.data(),
            "reset") == 0) {

        resetSpatialProfile();
        resetSpatialCommand();
        return;
    }

    ambilight::TofSpatialProfile profile;

    if (!parseSpatialProfileText(
            spatialCommandBuffer.data(),
            profile)) {

        Serial.println(
            "TOF SPATIAL command invalid. Example: y1437.5,1000,0,0,0,0,0,10");
        resetSpatialCommand();
        return;
    }

    applySpatialProfile(
        profile);

    resetSpatialCommand();
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

bool parseUint16Token(
    const char*& cursor,
    std::uint16_t& value) {

    if (cursor == nullptr ||
        *cursor < '0' ||
        *cursor > '9') {

        return false;
    }

    std::uint32_t parsed = 0;

    while (*cursor >= '0' &&
           *cursor <= '9') {

        parsed =
            parsed * 10U +
            static_cast<std::uint32_t>(
                *cursor - '0');

        if (parsed > 65535U) {
            return false;
        }

        ++cursor;
    }

    value =
        static_cast<std::uint16_t>(
            parsed);

    return true;
}

bool parseGainCurveText(
    const char* text,
    std::array<
        ambilight::GainPoint,
        ambilight::DistanceGainCurve::kMaxPoints>& points,
    std::size_t& count) {

    points = {};
    count = 0;

    if (text == nullptr ||
        *text == '\0') {
        return false;
    }

    const char* cursor = text;

    while (*cursor != '\0') {
        if (count >=
            points.size()) {

            return false;
        }

        std::uint16_t distance = 0;
        std::uint16_t gain = 0;

        if (!parseUint16Token(
                cursor,
                distance)) {
            return false;
        }

        if (*cursor != ':') {
            return false;
        }

        ++cursor;

        if (!parseUint16Token(
                cursor,
                gain)) {
            return false;
        }

        points[count++] = {
            distance,
            gain
        };

        if (*cursor == '\0') {
            break;
        }

        if (*cursor != ',') {
            return false;
        }

        ++cursor;

        if (*cursor == '\0') {
            return false;
        }
    }

    const ambilight::DistanceGainCurve curve(
        points,
        count);

    return curve.valid();
}

void invalidateRenderedGainAfterCurveChange() {
    cachedTargetGainContext =
        ambilight::RenderGainContext::unity();

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

void resetTofGainCurve() {
    if (correctionMode ==
        ambilight::CorrectionMode::Active) {

        Serial.println(
            "TOF CURVE reset refused in ACTIVE mode. Switch to SHADOW or DISABLED first.");
        return;
    }

    const bool persisted =
        runtimeSettings.resetTofGainCurve();

    const ambilight::DistanceGainCurve curve =
        runtimeSettings.tofGainCurve();

    if (!tof.setGainCurve(
            curve)) {

        Serial.println(
            "TOF CURVE reset stored, but sensor service could not queue it. Reboot will load the default.");
        return;
    }

    invalidateRenderedGainAfterCurveChange();

    Serial.printf(
        "TOF CURVE reset to default; persisted=%s. Waiting for next valid ToF pose rebuild.\n",
        persisted ? "yes" : "no");

    printTofGainCurve();
}

void resetGainCurveCommand() {
    gainCurveCommandPending = false;
    gainCurveCommandLength = 0;
    gainCurveCommandBuffer.fill('\0');
}

void finishGainCurveCommand() {
    gainCurveCommandBuffer[
        gainCurveCommandLength] = '\0';

    if (gainCurveCommandLength == 0) {
        printTofGainCurve();
        resetGainCurveCommand();
        return;
    }

    if (std::strcmp(
            gainCurveCommandBuffer.data(),
            "reset") == 0) {

        resetTofGainCurve();
        resetGainCurveCommand();
        return;
    }

    std::array<
        ambilight::GainPoint,
        ambilight::DistanceGainCurve::kMaxPoints>
        points{};

    std::size_t count = 0;

    if (!parseGainCurveText(
            gainCurveCommandBuffer.data(),
            points,
            count)) {

        Serial.println(
            "TOF CURVE command invalid. Example: q50:2048,500:3072,4000:4096");
        resetGainCurveCommand();
        return;
    }

    applyTofGainCurve(
        points,
        count);

    resetGainCurveCommand();
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

void refreshTargetGainContext(
    std::uint64_t nowUs) {

    if (nextGainTargetPollUs != 0 &&
        nowUs < nextGainTargetPollUs) {
        return;
    }

    if (!ambilight::CorrectionOutputPolicy::evaluatesGain(
            correctionMode)) {

        cachedTargetGainContext =
            ambilight::RenderGainContext::unity();

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
                nowUs);
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

    const bool gainDirty =
        correctionModeDirty ||
        brightnessDirty ||
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
            gainDirty,
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
        // histogram. Gain-only rerenders intentionally reuse an old RGB frame
        // and must not look like network queue latency.
        if (renderSnapshot.receivedUs != 0 &&
            renderSnapshot.receivedUs ==
                ddp.lastCompleteFrameUs()) {

            frameAgeHistogram.observe(
                lastFrameAgeUs);
        }
    }

    ambilight::RenderGainContext effectiveGainContext =
        ambilight::RenderGainContext::unity();

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
            ambilight::config::kTofRotationQuarterTurns % 4U),
        ambilight::config::kTofMirrorX ? "yes" : "no");

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

    for (const auto& segmentConfig :
         ambilight::kSegments) {

        if (segmentConfig.id != segment ||
            segmentConfig.logicalLength == 0) {
            continue;
        }

        const std::uint16_t midpoint =
            static_cast<std::uint16_t>(
                segmentConfig.logicalStart +
                segmentConfig.logicalLength / 2U);

        midpointQ12 =
            context.gainForLogicalIndex(
                midpoint);

        break;
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
        "RENDER SCHED rgb=%lu gain_only=%lu combined=%lu gain_deferred=%lu no_frame=%lu clean=%lu "
        "last_rgb_gen=%lu mailbox_gen=%lu\n",
        static_cast<unsigned long>(schedulerStats.rgbRenders),
        static_cast<unsigned long>(schedulerStats.gainOnlyRenders),
        static_cast<unsigned long>(schedulerStats.combinedRenders),
        static_cast<unsigned long>(schedulerStats.gainDeferrals),
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

    for (const auto& segmentConfig :
         ambilight::kSegments) {

        if (segmentConfig.id != segmentId ||
            segmentConfig.logicalLength == 0) {
            continue;
        }

        midpointQ12 =
            gains.logicalGainQ12[
                segmentConfig.logicalStart +
                segmentConfig.logicalLength / 2U];

        break;
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

void serviceDebugCommands() {
    while (Serial.available() > 0) {
        const int input = Serial.read();

        if (spatialCommandPending) {
            if (input == '\r' || input == '\n') {
                finishSpatialCommand();
                continue;
            }

            if (input < 32 || input > 126) {
                Serial.println(
                    "TOF SPATIAL command contains unsupported control characters.");
                resetSpatialCommand();
                continue;
            }

            if (spatialCommandLength + 1 >=
                spatialCommandBuffer.size()) {

                Serial.println(
                    "TOF SPATIAL command too long.");
                resetSpatialCommand();
                continue;
            }

            spatialCommandBuffer[
                spatialCommandLength++] =
                static_cast<char>(input);

            continue;
        }

        if (gainCurveCommandPending) {
            if (input == '\r' || input == '\n') {
                finishGainCurveCommand();
                continue;
            }

            if (input < 32 || input > 126) {
                Serial.println(
                    "TOF CURVE command contains unsupported control characters.");
                resetGainCurveCommand();
                continue;
            }

            if (gainCurveCommandLength + 1 >=
                gainCurveCommandBuffer.size()) {

                Serial.println(
                    "TOF CURVE command too long.");
                resetGainCurveCommand();
                continue;
            }

            gainCurveCommandBuffer[
                gainCurveCommandLength++] =
                static_cast<char>(input);

            continue;
        }

        if (wifiCommandPending) {
            if (input == '\r' || input == '\n') {
                finishWifiCommand();
                continue;
            }

            if (input < 32 || input > 126) {
                Serial.println(
                    "Wi-Fi command contains unsupported control characters.");
                resetWifiCommand();
                continue;
            }

            if (wifiCommandLength + 1 >=
                wifiCommandBuffer.size()) {

                Serial.println(
                    "Wi-Fi command too long.");
                resetWifiCommand();
                continue;
            }

            wifiCommandBuffer[
                wifiCommandLength++] =
                static_cast<char>(input);

            continue;
        }

        if (brightnessCommandPending) {
            if (input >= '0' && input <= '9') {
                if (brightnessCommandDigits >= 3) {
                    brightnessCommandPending = false;
                    brightnessCommandDigits = 0;
                    brightnessCommandValue = 0;

                    Serial.println(
                        "OUTPUT brightness command invalid. Use b0..b255.");
                    continue;
                }

                brightnessCommandValue =
                    static_cast<std::uint16_t>(
                        brightnessCommandValue * 10U +
                        static_cast<std::uint16_t>(
                            input - '0'));

                ++brightnessCommandDigits;
                continue;
            }

            if (input == '\r' || input == '\n') {
                if (brightnessCommandDigits == 0) {
                    printOutputBrightness();
                } else if (
                    brightnessCommandValue <= 255U) {

                    setOutputBrightness(
                        static_cast<std::uint8_t>(
                            brightnessCommandValue));
                } else {
                    Serial.println(
                        "OUTPUT brightness out of range. Use 0..255.");
                }

                brightnessCommandPending = false;
                brightnessCommandDigits = 0;
                brightnessCommandValue = 0;
                continue;
            }

            brightnessCommandPending = false;
            brightnessCommandDigits = 0;
            brightnessCommandValue = 0;

            Serial.println(
                "OUTPUT brightness command invalid. Use b0..b255.");
            continue;
        }

        if (correctionCommandPending) {
            correctionCommandPending = false;

            if (input == '0') {
                setCorrectionMode(
                    ambilight::CorrectionMode::Disabled);
            } else if (input == '1') {
                setCorrectionMode(
                    ambilight::CorrectionMode::Shadow);
            } else if (input == '2') {
                setCorrectionMode(
                    ambilight::CorrectionMode::Active);
            } else if (
                input != '\r' &&
                input != '\n') {

                Serial.println(
                    "CORRECTION command invalid. Use !0, !1 or !2.");
            }

            continue;
        }

        if (input == '!') {
            correctionCommandPending = true;
        } else if (input == 'y' || input == 'Y') {
            spatialCommandPending = true;
            spatialCommandLength = 0;
            spatialCommandBuffer.fill('\0');
        } else if (input == 'q' || input == 'Q') {
            gainCurveCommandPending = true;
            gainCurveCommandLength = 0;
            gainCurveCommandBuffer.fill('\0');
        } else if (input == 'w' || input == 'W') {
            wifiCommandPending = true;
            wifiCommandLength = 0;
            wifiCommandBuffer.fill('\0');
        } else if (input == 'b' || input == 'B') {
            brightnessCommandPending = true;
            brightnessCommandValue = 0;
            brightnessCommandDigits = 0;
        } else if (input == 't' || input == 'T') {
            dumpTofMap();
        } else if (input == 'g' || input == 'G') {
            dumpTofGeometry();
        } else if (input == 'p' || input == 'P') {
            dumpTofPlane();
        } else if (input == 'k' || input == 'K') {
            dumpTofGains();
        } else if (input == 's' || input == 'S') {
            dumpSpatialGains();
        } else if (input == 'c' || input == 'C') {
            startCalibrationCapture();
        } else if (input == 'r' || input == 'R') {
            dumpRenderShadow();
        } else if (input == 'x' || input == 'X') {
            startShadowGainProbe();
        } else if (input == 'm' || input == 'M') {
            printCorrectionMode();
        }
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
        "budget=%lu lim=%lu pollmax=%luus rxreq=%d rxactual=%d rxset=%s rxget=%s optwarn=%lu "
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
        "slew_snap=%lu probe=%s sched_rgb=%lu sched_gain=%lu sched_comb=%lu sched_def=%lu rgbgen=%lu "
        "tofinit=%lu toffail=%lu tofreadfail=%lu tofrestart=%lu black=%lu heap=%u minheap=%u\n",
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
        udp.requestedRxBufferBytes,
        udp.actualRxBufferBytes,
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
            renderScheduler.stats().gainOnlyRenders),
        static_cast<unsigned long>(
            renderScheduler.stats().combinedRenders),
        static_cast<unsigned long>(
            renderScheduler.stats().gainDeferrals),
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
        ESP.getFreeHeap(),
        ESP.getMinFreeHeap());

    Serial.printf(
        "STATCFG curve=%s curve_points=%u curve_updates=%lu spatial=%s spatial_updates=%lu\n",
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
            : 0UL);
}

void printConfiguration() {
    Serial.println();
    Serial.println(
        "ESP32-C6 Ambilight Stage 26: DDP socket hardening + sender isolation");

    Serial.printf(
        "Logical LEDs=%u payload=%uB DDP=%u poll_budget=%uus max_datagrams=%u\n",
        static_cast<unsigned>(ambilight::config::kLogicalLedCount),
        static_cast<unsigned>(
            ambilight::config::kLogicalLedCount *
            sizeof(ambilight::Rgb8)),
        ambilight::DdpUdpService::kPort,
        ambilight::DdpUdpService::kPollBudgetUs,
        static_cast<unsigned>(
            ambilight::DdpUdpService::kMaxDatagramsPerPoll));

    Serial.printf(
        "PARLIO lanes=%u lane_length=%u brightness=%u/255\n",
        static_cast<unsigned>(ambilight::config::kParlioLaneCount),
        static_cast<unsigned>(ambilight::config::kPhysicalLaneLength),
        static_cast<unsigned>(
            ledEngine.brightness()));

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
        "Debug: 't'=raw, 'g'=bands, 'p'=plane, 'k'=legacy gains, 's'=spatial gains, 'c'=capture, 'r'=render, 'x'=shadow probe.");
    Serial.println(
        "Output brightness: b0..b255 followed by Enter; b + Enter prints status.");
    Serial.println(
        "Wi-Fi: wSSID|PASSWORD + Enter sets/reconnects, w + Enter=status, wclear + Enter=clear NVS.");
    Serial.println(
        "ToF curve: q + Enter=status, qreset + Enter=default, qDIST:GAIN,... + Enter=set (not in ACTIVE).");
    Serial.println(
        "Spatial: y + Enter=status, yreset, or yW,H,X,Y,Z,ROT,MIRROR,DEADBAND (mm, not in ACTIVE).");
    Serial.println(
        "Active frame transport remains Wi-Fi/DDP only. "
        "USB/AWA is preserved separately as WIP.");
    Serial.println();
}

} // namespace

void setup() {
    Serial.begin(115200);
    delay(1000);

    if (!runtimeSettings.begin()) {
        Serial.println(
            "Runtime settings warning: NVS unavailable; using volatile defaults.");
    }

    correctionMode =
        runtimeSettings.correctionMode();

    ledEngine.setBrightness(
        runtimeSettings.outputBrightness());

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
    } else {
        Serial.println(
            "DDP runtime inactive because Wi-Fi credentials are absent. Provision with wSSID|PASSWORD.");
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

    if (pollResult.backlogLikely &&
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

    serviceIdleBlackout(nowUs);
    serviceRender(nowUs);

    const std::uint32_t nowMs = millis();
    if (static_cast<std::int32_t>(
            nowMs - (lastStatusMs + kStatusIntervalMs)) >= 0) {
        printRuntimeStatus();
        lastStatusMs = nowMs;
    }

    delay(1);
}
