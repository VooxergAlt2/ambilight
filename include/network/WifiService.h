#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

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

    const char* ssid() const {
        return ssid_.data();
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

    bool enabled_ = false;
    bool wasConnected_ = false;

    std::array<char, kMaxSsidLength + 1> ssid_{};
    std::array<char, kMaxPasswordLength + 1> password_{};

    std::uint32_t nextReconnectMs_ = 0;
    std::uint32_t reconnectAttempts_ = 0;
    std::uint32_t connectEvents_ = 0;
    std::uint32_t disconnectEvents_ = 0;
};

} // namespace ambilight
