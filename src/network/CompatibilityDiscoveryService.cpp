#include "network/CompatibilityDiscoveryService.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#include <cstddef>
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

bool normalizedMac(
    char* output,
    std::size_t capacity) {

    if (output == nullptr ||
        capacity < 13U) {

        return false;
    }

    const String mac =
        WiFi.macAddress();

    std::size_t written = 0;

    for (std::size_t index = 0;
         index <
             static_cast<std::size_t>(
                 mac.length()) &&
         written < 12U;
         ++index) {

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

        const bool hex =
            (value >= '0' &&
             value <= '9') ||
            (value >= 'a' &&
             value <= 'f');

        if (!hex) {
            return false;
        }

        output[written++] =
            value;
    }

    if (written != 12U) {
        return false;
    }

    output[written] = '\0';
    return true;
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

    const bool httpAdded =
        MDNS.addService(
            "http",
            "tcp",
            80);

    const bool wledAdded =
        MDNS.addService(
            "wled",
            "tcp",
            80);

    if (!httpAdded ||
        !wledAdded) {

        MDNS.end();
        ++stats_.startFailures;
        return false;
    }

    // Match the real WLED discovery contract used by Home Assistant.
    // The TXT record lets HA deduplicate the device before its first /json
    // request. A failed MAC normalization is non-fatal because /json still
    // exposes the canonical MAC and remains the final authority.
    char mac[13] = {};

    if (normalizedMac(
            mac,
            sizeof(mac))) {

        const char* const macTxt = mac;

        MDNS.addServiceTxt(
            "wled",
            "tcp",
            "mac",
            macTxt);
    }

    running_ = true;
    ++stats_.starts;

    Serial.printf(
        "Compatibility discovery started: hostname=%s services=_http._tcp,_wled._tcp port=80.\n",
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
