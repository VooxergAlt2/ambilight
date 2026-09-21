#include "network/CompatibilityDiscoveryService.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#include <cstdio>

namespace ambilight {
namespace {

constexpr std::uint32_t
    kRetryIntervalMs = 5000;

bool deadlineReached(
    std::uint32_t nowMs,
    std::uint32_t deadlineMs) {

    return
        static_cast<std::int32_t>(
            nowMs - deadlineMs) >= 0;
}

} // namespace

void CompatibilityDiscoveryService::buildHostname() {
    hostname_.fill('\0');

    const String mac =
        WiFi.macAddress();

    char suffix[7] = {};
    std::size_t written = 0;

    for (
        int index =
            static_cast<int>(
                mac.length()) - 1;
        index >= 0 &&
        written < 6;
        --index
    ) {

        char value =
            mac[
                static_cast<unsigned>(
                    index)];

        if (value == ':' ||
            value == '-' ||
            value == '.') {

            continue;
        }

        if (value >= 'A' &&
            value <= 'F') {

            value =
                static_cast<char>(
                    value - 'A' + 'a');
        }

        suffix[
            5U - written] =
            value;

        ++written;
    }

    if (written != 6) {
        std::snprintf(
            hostname_.data(),
            hostname_.size(),
            "ambilight-c6");
        return;
    }

    std::snprintf(
        hostname_.data(),
        hostname_.size(),
        "ambilight-c6-%s",
        suffix);
}

bool CompatibilityDiscoveryService::start() {
    buildHostname();

    if (!MDNS.begin(
            hostname_.data())) {

        ++stats_.startFailures;
        return false;
    }

    MDNS.setInstanceName(
        "Ambilight C6");

    const bool wledAdded =
        MDNS.addService(
            "wled",
            "tcp",
            80);

    const bool hyperkAdded =
        MDNS.addService(
            "hyperk",
            "tcp",
            80);

    if (!wledAdded ||
        !hyperkAdded) {

        MDNS.end();
        ++stats_.startFailures;
        return false;
    }

    running_ = true;
    ++stats_.starts;

    Serial.printf(
        "Compatibility discovery started: hostname=%s services=_wled._tcp,_hyperk._tcp port=80.\n",
        hostname_.data());

    return true;
}

void CompatibilityDiscoveryService::stop() {
    if (!running_) {
        return;
    }

    MDNS.end();

    running_ = false;
    hostname_.fill('\0');
    ++stats_.stops;

    Serial.println(
        "Compatibility discovery stopped.");
}

void CompatibilityDiscoveryService::tick(
    bool networkReady,
    std::uint32_t nowMs) {

    if (!networkReady) {
        stop();
        nextStartAttemptMs_ = nowMs;
        return;
    }

    if (running_) {
        return;
    }

    if (!deadlineReached(
            nowMs,
            nextStartAttemptMs_)) {

        return;
    }

    if (!start()) {
        nextStartAttemptMs_ =
            nowMs +
            kRetryIntervalMs;
        return;
    }

    nextStartAttemptMs_ = 0;
}

} // namespace ambilight
