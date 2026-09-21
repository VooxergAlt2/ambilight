#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ambilight {

struct CompatibilityDiscoveryStats {
    std::uint32_t starts = 0;
    std::uint32_t startFailures = 0;
    std::uint32_t stops = 0;
};

class CompatibilityDiscoveryService {
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

    const CompatibilityDiscoveryStats& stats() const {
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

    CompatibilityDiscoveryStats stats_{};
};

} // namespace ambilight
