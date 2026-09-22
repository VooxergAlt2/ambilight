#include "network/WifiService.h"

#include <Arduino.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <WiFi.h>
#include <esp_err.h>
#include <esp_wifi.h>


namespace ambilight {
namespace {

constexpr std::uint32_t kReconnectIntervalMs = 5000;
constexpr const char kFallbackApPassword[] = "ambilight";
constexpr std::uint8_t kFallbackApChannel = 1;
constexpr std::uint8_t kFallbackApMaxClients = 4;

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

        if (accessPointActive_) {
            if (enabled_) {
                WiFi.disconnect(
                    false,
                    false);
            }

            enabled_ = false;
            wasConnected_ = false;
            nextReconnectMs_ = 0;
            fallbackPolicy_.cancel();

            ssid_.fill('\0');
            password_.fill('\0');

            WiFi.mode(
                WIFI_AP);

            Serial.println(
                "Wi-Fi station disabled: fallback AP remains active for provisioning.");
            return true;
        }

        disable();
        configureAccessPointIdentity();
        armFallback(
            millis());
        Serial.println(
            "Wi-Fi station disabled: no credentials configured; fallback AP will open after 60 s.");
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
    WiFi.mode(
        accessPointActive_
            ? WIFI_AP_STA
            : WIFI_STA);
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

    configureAccessPointIdentity();
    armFallback(
        millis());

    Serial.printf(
        "Wi-Fi connecting to SSID '%s' with power-save disabled.\n",
        ssid_.data());

    return true;
}

void WifiService::disable() {
    if (accessPointActive_) {
        WiFi.softAPdisconnect(false);
    }

    if (enabled_ ||
        accessPointActive_) {
        WiFi.disconnect(
            true,
            false);

        WiFi.mode(
            WIFI_OFF);
    }

    enabled_ = false;
    wasConnected_ = false;
    accessPointActive_ = false;
    nextReconnectMs_ = 0;
    fallbackPolicy_.cancel();

    ssid_.fill('\0');
    password_.fill('\0');
    accessPointIp_.fill('\0');
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

void WifiService::configureAccessPointIdentity() {
    const std::uint64_t chipId =
        static_cast<std::uint64_t>(
            ESP.getEfuseMac());

    std::snprintf(
        accessPointSsid_.data(),
        accessPointSsid_.size(),
        "Ambilight-%06llX",
        static_cast<unsigned long long>(
            chipId & 0xFFFFFFULL));
}

void WifiService::armFallback(
    std::uint32_t nowMs) {

    fallbackPolicy_.arm(
        nowMs);
}

bool WifiService::startFallbackAccessPoint(
    std::uint32_t nowMs) {

    if (accessPointActive_) {
        fallbackPolicy_.cancel();
        return true;
    }

    if (accessPointSsid_[0] == '\0') {
        configureAccessPointIdentity();
    }

    WiFi.persistent(false);

    if (!WiFi.mode(
            enabled_
                ? WIFI_AP_STA
                : WIFI_AP)) {

        Serial.println(
            "Wi-Fi fallback AP failed: could not enable AP mode.");
        fallbackPolicy_.retryLater(
            nowMs);
        return false;
    }

    const IPAddress apIp(
        4,
        3,
        2,
        1);

    const IPAddress subnet(
        255,
        255,
        255,
        0);

    if (!WiFi.softAPConfig(
            apIp,
            apIp,
            subnet) ||
        !WiFi.softAP(
            accessPointSsid_.data(),
            kFallbackApPassword,
            kFallbackApChannel,
            0,
            kFallbackApMaxClients)) {

        Serial.println(
            "Wi-Fi fallback AP failed to start; retrying in 5 s.");

        WiFi.mode(
            enabled_
                ? WIFI_STA
                : WIFI_OFF);

        fallbackPolicy_.retryLater(
            nowMs);
        return false;
    }

    accessPointActive_ = true;
    fallbackPolicy_.cancel();

    const String ip =
        WiFi.softAPIP().toString();

    std::snprintf(
        accessPointIp_.data(),
        accessPointIp_.size(),
        "%s",
        ip.c_str());

    Serial.printf(
        "Wi-Fi fallback AP started: SSID='%s' password='%s' IP=%s. STA reconnect remains active.\n",
        accessPointSsid_.data(),
        kFallbackApPassword,
        accessPointIp_.data());

    return true;
}

void WifiService::stopFallbackAccessPoint() {
    if (!accessPointActive_) {
        return;
    }

    WiFi.softAPdisconnect(false);

    WiFi.mode(
        enabled_
            ? WIFI_STA
            : WIFI_OFF);

    accessPointActive_ = false;
    accessPointIp_.fill('\0');

    Serial.println(
        "Wi-Fi fallback AP stopped after STA connection.");
}

void WifiService::updateConnectionState(std::uint32_t nowMs) {
    const bool isConnected = connected();

    if (isConnected != wasConnected_) {
        if (isConnected) {
            fallbackPolicy_.cancel();
            stopFallbackAccessPoint();

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
            armFallback(
                nowMs);
        }

        wasConnected_ = isConnected;
    }

    if (!isConnected && deadlineReached(nowMs, nextReconnectMs_)) {
        requestReconnect(nowMs);
    }

    if (!isConnected &&
        !accessPointActive_ &&
        fallbackPolicy_.due(
            nowMs)) {

        startFallbackAccessPoint(
            nowMs);
    }
}

void WifiService::tick(std::uint32_t nowMs) {
    if (!enabled_) {
        if (!accessPointActive_ &&
            fallbackPolicy_.due(
                nowMs)) {

            startFallbackAccessPoint(
                nowMs);
        }

        return;
    }

    updateConnectionState(nowMs);
}

void WifiService::printStatus() const {
    if (!enabled_) {
        Serial.printf(
            "Wi-Fi status: station disabled fallback_ap=%s ap_ssid='%s' ap_ip=%s\n",
            accessPointActive_ ? "on" : "off",
            accessPointSsid_.data(),
            accessPointActive_
                ? accessPointIp_.data()
                : "-");
        return;
    }

    Serial.printf(
        "Wi-Fi status: ssid='%s' connected=%s status=%d RSSI=%d reconnects=%lu connects=%lu disconnects=%lu fallback_ap=%s ap_ssid='%s' ap_ip=%s\n",
        ssid_.data(),
        connected() ? "yes" : "no",
        static_cast<int>(WiFi.status()),
        connected() ? WiFi.RSSI() : 0,
        static_cast<unsigned long>(reconnectAttempts_),
        static_cast<unsigned long>(connectEvents_),
        static_cast<unsigned long>(disconnectEvents_),
        accessPointActive_ ? "on" : "off",
        accessPointSsid_.data(),
        accessPointActive_
            ? accessPointIp_.data()
            : "-");
}

} // namespace ambilight
