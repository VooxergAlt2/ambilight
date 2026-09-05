#pragma once

#include <cstdint>

namespace ambilight {

class WifiService {
public:
    bool begin();
    void tick(std::uint32_t nowMs);
    void printStatus() const;

    bool enabled() const { return enabled_; }
    bool connected() const;

    std::uint32_t reconnectAttempts() const { return reconnectAttempts_; }
    std::uint32_t connectEvents() const { return connectEvents_; }
    std::uint32_t disconnectEvents() const { return disconnectEvents_; }

private:
    void updateConnectionState(std::uint32_t nowMs);
    void requestReconnect(std::uint32_t nowMs);

    bool enabled_ = false;
    bool wasConnected_ = false;

    std::uint32_t nextReconnectMs_ = 0;
    std::uint32_t reconnectAttempts_ = 0;
    std::uint32_t connectEvents_ = 0;
    std::uint32_t disconnectEvents_ = 0;
};

} // namespace ambilight
