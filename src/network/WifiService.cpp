#include "network/WifiService.h"

#include <Arduino.h>

#include <algorithm>
#include <cstring>
#include <WiFi.h>
#include <esp_err.h>
#include <esp_wifi.h>


namespace ambilight {
namespace {

constexpr std::uint32_t kReconnectIntervalMs = 5000;

bool deadlineReached(std::uint32_t nowMs, std::uint32_t deadlineMs) {
    return static_cast<std::int32_t>(nowMs - deadlineMs) >= 0;
}

} // namespace

bool WifiService::credentialsValid(
    const char* ssid,
    const char* password) {

    if (ssid == nullptr) {
        return false;
    }

    if (password == nullptr) {
        password = "";
    }

    const std::size_t ssidLength =
        std::strlen(ssid);

    const std::size_t passwordLength =
        std::strlen(password);

    return
        ssidLength > 0 &&
        ssidLength <= kMaxSsidLength &&
        passwordLength <=
            kMaxPasswordLength;
}

void WifiService::copyText(
    std::array<char, kMaxSsidLength + 1>& destination,
    const char* source) {

    destination.fill('\0');

    if (source == nullptr) {
        return;
    }

    const std::size_t length =
        std::min<std::size_t>(
            std::strlen(source),
            kMaxSsidLength);

    std::memcpy(
        destination.data(),
        source,
        length);

    destination[length] = '\0';
}

void WifiService::copyPassword(
    std::array<char, kMaxPasswordLength + 1>& destination,
    const char* source) {

    destination.fill('\0');

    if (source == nullptr) {
        return;
    }

    const std::size_t length =
        std::min<std::size_t>(
            std::strlen(source),
            kMaxPasswordLength);

    std::memcpy(
        destination.data(),
        source,
        length);

    destination[length] = '\0';
}

bool WifiService::begin(
    const char* ssid,
    const char* password) {

    if (ssid == nullptr ||
        ssid[0] == '\0') {

        disable();
        Serial.println(
            "Wi-Fi disabled: no credentials configured.");
        return true;
    }

    return configure(
        ssid,
        password);
}

bool WifiService::configure(
    const char* ssid,
    const char* password) {

    if (!credentialsValid(
            ssid,
            password)) {

        return false;
    }

    if (password == nullptr) {
        password = "";
    }

    if (enabled_) {
        WiFi.disconnect(
            false,
            false);
    }

    copyText(
        ssid_,
        ssid);

    copyPassword(
        password_,
        password);

    enabled_ = true;
    wasConnected_ = false;

    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);

    WiFi.begin(
        ssid_.data(),
        password_.data());

    const esp_err_t psResult =
        esp_wifi_set_ps(
            WIFI_PS_NONE);

    if (psResult != ESP_OK) {
        Serial.printf(
            "Wi-Fi warning: esp_wifi_set_ps(WIFI_PS_NONE) failed: %s\n",
            esp_err_to_name(psResult));
    }

    nextReconnectMs_ =
        millis() +
        kReconnectIntervalMs;

    Serial.printf(
        "Wi-Fi connecting to SSID '%s' with power-save disabled.\n",
        ssid_.data());

    return true;
}

void WifiService::disable() {
    if (enabled_) {
        WiFi.disconnect(
            true,
            false);

        WiFi.mode(
            WIFI_OFF);
    }

    enabled_ = false;
    wasConnected_ = false;
    nextReconnectMs_ = 0;

    ssid_.fill('\0');
    password_.fill('\0');
}

bool WifiService::connected() const {
    return enabled_ && WiFi.status() == WL_CONNECTED;
}

void WifiService::requestReconnect(std::uint32_t nowMs) {
    if (!enabled_) {
        return;
    }

    ++reconnectAttempts_;
    WiFi.reconnect();
    nextReconnectMs_ = nowMs + kReconnectIntervalMs;
}

void WifiService::updateConnectionState(std::uint32_t nowMs) {
    const bool isConnected = connected();

    if (isConnected != wasConnected_) {
        if (isConnected) {
            ++connectEvents_;
            Serial.printf(
                "Wi-Fi connected: IP=%s RSSI=%d dBm channel=%ld\n",
                WiFi.localIP().toString().c_str(),
                WiFi.RSSI(),
                static_cast<long>(WiFi.channel()));
        } else {
            ++disconnectEvents_;
            Serial.printf(
                "Wi-Fi disconnected: status=%d\n",
                static_cast<int>(WiFi.status()));
            nextReconnectMs_ = nowMs + kReconnectIntervalMs;
        }

        wasConnected_ = isConnected;
    }

    if (!isConnected && deadlineReached(nowMs, nextReconnectMs_)) {
        requestReconnect(nowMs);
    }
}

void WifiService::tick(std::uint32_t nowMs) {
    if (!enabled_) {
        return;
    }

    updateConnectionState(nowMs);
}

void WifiService::printStatus() const {
    if (!enabled_) {
        Serial.println("Wi-Fi status: disabled");
        return;
    }

    Serial.printf(
        "Wi-Fi status: ssid='%s' connected=%s status=%d RSSI=%d reconnects=%lu connects=%lu disconnects=%lu\n",
        ssid_.data(),
        connected() ? "yes" : "no",
        static_cast<int>(WiFi.status()),
        connected() ? WiFi.RSSI() : 0,
        static_cast<unsigned long>(reconnectAttempts_),
        static_cast<unsigned long>(connectEvents_),
        static_cast<unsigned long>(disconnectEvents_));
}

} // namespace ambilight
