#include <Arduino.h>
#include <WiFi.h>
#include <esp_err.h>
#include <esp_timer.h>
#include <lwip/inet.h>

#include "config/BoardConfig.h"
#include "core/FrameMailbox.h"
#include "core/LatencyHistogram.h"
#include "core/RgbFrame.h"
#include "led/LedEngine.h"
#include "led/LedRenderer.h"
#include "integration/TofRenderGainBridge.h"
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
constexpr std::uint64_t kGainTargetPollIntervalUs = 10000;

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

ambilight::RgbFrame renderSnapshot;
ambilight::GainSnapshot cachedGainSnapshot{};
ambilight::RenderGainContext cachedTargetGainContext{};

bool rgbFrameValid = false;
bool rgbDirty = false;
bool haveCachedGainSnapshot = false;

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

    ambilight::GainSnapshot latest{};

    if (tof.copyGainSnapshot(latest)) {
        cachedGainSnapshot = latest;
        haveCachedGainSnapshot = true;
    }

    cachedTargetGainContext =
        ambilight::TofRenderGainBridge::make(
            cachedGainSnapshot,
            haveCachedGainSnapshot,
            nowUs);

    if (shadowProbeUntilUs != 0 &&
        nowUs < shadowProbeUntilUs) {

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

    const bool targetProfileChanged =
        !cachedTargetGainContext.sameRenderProfileAs(
            renderGainController.target());

    const bool gainDirty =
        targetProfileChanged ||
        !renderGainController.settled();

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

    const ambilight::RenderGainContext effectiveGainContext =
        renderGainController.update(
            cachedTargetGainContext,
            nowUs);

    const esp_err_t result =
        renderer.render(
            renderSnapshot,
            effectiveGainContext);

    if (result != ESP_OK) {
        fatal(
            "LedRenderer::render failed",
            result);
    }

    renderScheduler.markRendered(nowUs);

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

    Serial.printf(
        "RENDER %s start=%u(%lu.%lu%%) end=%u(%lu.%lu%%)\n",
        name,
        endpoints.startQ12,
        static_cast<unsigned long>(
            gainPercentX10(endpoints.startQ12) / 10U),
        static_cast<unsigned long>(
            gainPercentX10(endpoints.startQ12) % 10U),
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
        "RENDER SHADOW frames=%lu present=%lu usable=%lu failopen=%lu nonunity=%lu "
        "last_changed=%u max_delta=%u shadow_rgb=%lu.%lu%% source_gen=%lu source_age=%lluus "
        "prepare=%luus prepare_max=%luus\n",
        static_cast<unsigned long>(stats.frames),
        static_cast<unsigned long>(stats.sourcePresentFrames),
        static_cast<unsigned long>(stats.sourceUsableFrames),
        static_cast<unsigned long>(stats.failOpenFrames),
        static_cast<unsigned long>(stats.nonUnityContextFrames),
        stats.lastWouldChangePixels,
        static_cast<unsigned>(stats.lastMaxChannelDelta),
        static_cast<unsigned long>(shadowPermille / 10U),
        static_cast<unsigned long>(shadowPermille % 10U),
        static_cast<unsigned long>(stats.lastSourceGeneration),
        static_cast<unsigned long long>(stats.lastSourceAgeUs),
        static_cast<unsigned long>(stats.lastPrepareUs),
        static_cast<unsigned long>(stats.maxPrepareUs));

    Serial.printf(
        "RENDER SHADOW cumulative_changed=%llu max_delta_ever=%u by_segment T=%llu R=%llu B=%llu L=%llu\n",
        static_cast<unsigned long long>(
            stats.wouldChangePixels),
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
        static_cast<unsigned>(controllerStats.maxEndpointStepQ12));

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

    Serial.println(
        "RENDER SHADOW SAFETY: wouldOutput is calculated but original HyperHDR RGB is what reaches LedEngine.");
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

    Serial.println(
        "Stage 11 note: GainSnapshot reaches LedRenderer only through shadow context; ShadowRenderPolicy still outputs original RGB.");
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

    Serial.println(
        "CAL note: pair this summary with the physical TV pose; RGB is unchanged.");
    Serial.println();
}

void startCalibrationCapture() {
    const std::uint64_t nowUs =
        static_cast<std::uint64_t>(esp_timer_get_time());

    calibrationLastGeometryGeneration = 0;
    calibrationCapture.start(nowUs);

    Serial.println(
        "CAL started: collecting 5 seconds of unique ToF geometry. Keep the TV still.");
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
            snapshot.geometry);
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

        if (input == 't' || input == 'T') {
            dumpTofMap();
        } else if (input == 'g' || input == 'G') {
            dumpTofGeometry();
        } else if (input == 'k' || input == 'K') {
            dumpTofGains();
        } else if (input == 'c' || input == 'C') {
            startCalibrationCapture();
        } else if (input == 'r' || input == 'R') {
            dumpRenderShadow();
        } else if (input == 'x' || input == 'X') {
            startShadowGainProbe();
        }
    }
}

void printRuntimeStatus() {
    char senderIp[INET_ADDRSTRLEN] = "none";

    const std::uint32_t sender =
        ddp.lastSenderIpv4NetworkOrder();

    if (sender != 0) {
        in_addr address{};
        address.s_addr = sender;
        inet_ntop(AF_INET, &address, senderIp, sizeof(senderIp));
    }

    const auto& udp = ddp.stats();
    const auto& asmStats = ddp.assemblerStats();

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
        "STAT wifi=%s rssi=%d pkt=%lu asm=%lu pub=%lu collapse=%lu rej=%lu stale=%lu timeout=%lu "
        "budget=%lu lim=%lu pollmax=%luus sender=%s:%u render=%lu skip=%lu "
        "p50<=%luus p95<=%luus p99<=%luus ovf=%llu agemax=%lluus showmax=%luus "
        "tof=%s tofgen=%lu rawvalid=%u rawmed=%u tofage=%llums tofread=%luus tofreadmax=%luus "
        "geom=%s l=%u c=%u r=%u delta=%d acc=%u gainfail=%s gl=%u gt=%u gb=%u gr=%u "
        "shadow_usable=%lu shadow_nonunity=%lu shadow_changed=%u shadow_delta=%u shadowprep=%luus "
        "slew_snap=%lu probe=%s sched_rgb=%lu sched_gain=%lu sched_comb=%lu sched_def=%lu rgbgen=%lu "
        "tofinit=%lu toffail=%lu tofreadfail=%lu tofrestart=%lu black=%lu heap=%u minheap=%u\n",
        wifi.connected() ? "up" : "down",
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
        senderIp,
        ddp.lastSenderPort(),
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
}

void printConfiguration() {
    Serial.println();
    Serial.println(
        "ESP32-C6 Ambilight Stage 13: Wi-Fi/DDP + render dirty scheduler");

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
        static_cast<unsigned>(ambilight::config::kTestBrightness));

    Serial.printf(
        "VL53L5CX SDA=%u SCL=%u 8x8@10Hz rotation=%u mirror_x=%s; "
        "gain model enters renderer in SHADOW mode and does NOT modify physical RGB.\n",
        ambilight::config::kTofSdaGpio,
        ambilight::config::kTofSclGpio,
        static_cast<unsigned>(
            ambilight::config::kTofRotationQuarterTurns % 4U),
        ambilight::config::kTofMirrorX ? "yes" : "no");

    Serial.println(
        "Debug: 't'=raw, 'g'=geometry, 'k'=gains, 'c'=capture, 'r'=shadow/scheduler, 'x'=10s shadow probe.");
    Serial.println(
        "Active frame transport remains Wi-Fi/DDP only. "
        "USB/AWA is preserved separately as WIP.");
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
            "DDP socket bound to UDP/%u. Waiting for HyperHDR.\n",
            ambilight::DdpUdpService::kPort);
    } else {
        Serial.println(
            "DDP runtime inactive because Wi-Fi credentials are absent.");
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
