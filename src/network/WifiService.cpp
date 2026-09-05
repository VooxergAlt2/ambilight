#include "network/WifiService.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_err.h>
#include <esp_wifi.h>

#include "config/WifiCredentials.h"

namespace ambilight {
namespace {

constexpr std::uint32_t kReconnectIntervalMs = 5000;

bool deadlineReached(std::uint32_t nowMs, std::uint32_t deadlineMs) {
    return static_cast<std::int32_t>(nowMs - deadlineMs) >= 0;
}

} // namespace

bool WifiService::begin() {
    if (!config::wifiCredentialsPresent()) {
        Serial.println("Wi-Fi disabled: include/secrets.h is not configured.");
        enabled_ = false;
        return true;
    }

    enabled_ = true;

    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    // Realtime DDP cares about latency/jitter more than power consumption.
    WiFi.setSleep(false);

    WiFi.begin(config::kWifiSsid, config::kWifiPassword);

    const esp_err_t psResult = esp_wifi_set_ps(WIFI_PS_NONE);
    if (psResult != ESP_OK) {
        Serial.printf(
            "Wi-Fi warning: esp_wifi_set_ps(WIFI_PS_NONE) failed: %s\n",
            esp_err_to_name(psResult));
    }

    nextReconnectMs_ = millis() + kReconnectIntervalMs;

    Serial.printf(
        "Wi-Fi connecting to SSID '%s' with power-save disabled.\n",
        config::kWifiSsid);

    return true;
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
                "Wi-Fi connected: IP=%s RSSI=%d dBm channel=%d\n",
                WiFi.localIP().toString().c_str(),
                WiFi.RSSI(),
                WiFi.channel());
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
        "Wi-Fi status: connected=%s status=%d RSSI=%d reconnects=%lu connects=%lu disconnects=%lu\n",
        connected() ? "yes" : "no",
        static_cast<int>(WiFi.status()),
        connected() ? WiFi.RSSI() : 0,
        static_cast<unsigned long>(reconnectAttempts_),
        static_cast<unsigned long>(connectEvents_),
        static_cast<unsigned long>(disconnectEvents_));
}

} // namespace ambilight
