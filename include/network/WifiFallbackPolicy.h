#pragma once

#include <cstdint>

namespace ambilight {

// Wrap-safe grace timer used by WifiService before exposing the fallback AP.
// Keeping the timing policy independent from Arduino Wi-Fi APIs makes the
// boot/disconnect behaviour part of the native regression suite.
class WifiFallbackPolicy {
public:
    static constexpr std::uint32_t kDelayMs = 60000U;
    static constexpr std::uint32_t kRetryDelayMs = 5000U;

    void arm(std::uint32_t nowMs) {
        armed_ = true;
        deadlineMs_ = nowMs + kDelayMs;
    }

    void retryLater(std::uint32_t nowMs) {
        armed_ = true;
        deadlineMs_ = nowMs + kRetryDelayMs;
    }

    void cancel() {
        armed_ = false;
        deadlineMs_ = 0;
    }

    bool armed() const {
        return armed_;
    }

    bool due(std::uint32_t nowMs) const {
        return
            armed_ &&
            static_cast<std::int32_t>(
                nowMs - deadlineMs_) >= 0;
    }

    std::uint32_t deadlineMs() const {
        return deadlineMs_;
    }

private:
    bool armed_ = false;
    std::uint32_t deadlineMs_ = 0;
};

} // namespace ambilight
