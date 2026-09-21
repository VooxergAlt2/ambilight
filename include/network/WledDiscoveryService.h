#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ambilight {

struct WledDiscoveryStats {
    std::uint32_t starts = 0;
    std::uint32_t startFailures = 0;
    std::uint32_t stops = 0;
};

class WledDiscoveryService {
public:
    static constexpr std::size_t
        kHostnameCapacity = 32;

    void tick(
        bool networkReady,
        std::uint32_t nowMs);

    void stop();

    bool running() const {
        return running_;
    }

    const char* hostname() const {
        return hostname_.data();
    }

    const WledDiscoveryStats& stats() const {
        return stats_;
    }

private:
    bool start();
    void buildHostname();

    bool running_ = false;

    std::uint32_t nextStartAttemptMs_ = 0;

    std::array<
        char,
        kHostnameCapacity>
        hostname_{};

    WledDiscoveryStats stats_{};
};

} // namespace ambilight
