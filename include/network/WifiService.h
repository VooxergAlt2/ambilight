#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "network/WifiFallbackPolicy.h"

namespace ambilight {

class WifiService {
public:
    static constexpr std::size_t kMaxSsidLength = 32;
    static constexpr std::size_t kMaxPasswordLength = 63;

    bool begin(
        const char* ssid,
        const char* password);

    bool configure(
        const char* ssid,
        const char* password);

    void disable();

    void tick(std::uint32_t nowMs);
    void printStatus() const;

    bool enabled() const { return enabled_; }
    bool connected() const;

    bool accessPointActive() const {
        return accessPointActive_;
    }

    bool networkRuntimeEnabled() const {
        return enabled_ || accessPointActive_;
    }

    const char* ssid() const {
        return ssid_.data();
    }

    const char* accessPointSsid() const {
        return accessPointSsid_.data();
    }

    const char* accessPointIp() const {
        return accessPointIp_.data();
    }

    std::uint32_t reconnectAttempts() const { return reconnectAttempts_; }
    std::uint32_t connectEvents() const { return connectEvents_; }
    std::uint32_t disconnectEvents() const { return disconnectEvents_; }

private:
    static bool credentialsValid(
        const char* ssid,
        const char* password);

    static void copyText(
        std::array<char, kMaxSsidLength + 1>& destination,
        const char* source);

    static void copyPassword(
        std::array<char, kMaxPasswordLength + 1>& destination,
        const char* source);

    void updateConnectionState(std::uint32_t nowMs);
    void requestReconnect(std::uint32_t nowMs);
    bool startFallbackAccessPoint(std::uint32_t nowMs);
    void stopFallbackAccessPoint();
    void armFallback(std::uint32_t nowMs);
    void configureAccessPointIdentity();

    bool enabled_ = false;
    bool wasConnected_ = false;
    bool accessPointActive_ = false;

    std::array<char, kMaxSsidLength + 1> ssid_{};
    std::array<char, kMaxPasswordLength + 1> password_{};
    std::array<char, kMaxSsidLength + 1> accessPointSsid_{};
    std::array<char, 16> accessPointIp_{};

    WifiFallbackPolicy fallbackPolicy_{};

    std::uint32_t nextReconnectMs_ = 0;
    std::uint32_t reconnectAttempts_ = 0;
    std::uint32_t connectEvents_ = 0;
    std::uint32_t disconnectEvents_ = 0;
};

} // namespace ambilight
