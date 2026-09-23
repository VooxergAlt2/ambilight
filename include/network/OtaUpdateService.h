#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

class WebServer;

namespace ambilight {

struct OtaUpdateStats {
    std::uint32_t starts = 0;
    std::uint32_t startFailures = 0;
    std::uint32_t arms = 0;
    std::uint32_t uploadsStarted = 0;
    std::uint32_t uploadsCompleted = 0;
    std::uint32_t uploadsRejected = 0;
    std::uint32_t writeFailures = 0;
};

class OtaUpdateService {
public:
    static constexpr std::uint16_t kPort = 3232;
    static constexpr std::uint64_t kArmDurationUs = 120000000ULL;
    static constexpr std::uint64_t kRebootDelayUs = 1000000ULL;

    OtaUpdateService() = default;
    ~OtaUpdateService();

    OtaUpdateService(const OtaUpdateService&) = delete;
    OtaUpdateService& operator=(const OtaUpdateService&) = delete;

    bool begin();
    void stop();
    void tick(std::uint64_t nowUs);

    bool arm(std::uint64_t nowUs);
    void disarm();

    bool running() const {
        return running_;
    }

    bool armed(std::uint64_t nowUs) const;

    bool inProgress() const {
        return inProgress_;
    }

    bool rebootPending() const {
        return rebootPending_;
    }

    bool rebootReady(std::uint64_t nowUs) const {
        return
            rebootPending_ &&
            successAtUs_ != 0 &&
            nowUs >= successAtUs_ &&
            nowUs - successAtUs_ >=
                kRebootDelayUs;
    }

    const char* token() const {
        return token_.data();
    }

    std::uint64_t armedUntilUs() const {
        return armedUntilUs_;
    }

    std::size_t receivedBytes() const {
        return receivedBytes_;
    }

    bool lastSuccess() const {
        return lastSuccess_;
    }

    const char* lastMessage() const {
        return lastMessage_.data();
    }

    const OtaUpdateStats& stats() const {
        return stats_;
    }

private:
    void resetUploadState();
    void failUpload(
        int httpStatus,
        const char* message,
        bool abortUpdate = true);
    bool validateAndStartUpdate();
    bool writeBytes(
        std::uint8_t* data,
        std::size_t length);
    bool tokenMatchesRequest() const;
    void addCorsHeaders();
    void handleOptions();
    void handleUploadChunk();
    void handleUploadComplete();

    WebServer* server_ = nullptr;
    bool running_ = false;

    std::array<char, 33> token_{};
    std::uint64_t armedUntilUs_ = 0;

    bool inProgress_ = false;
    bool uploadAuthorized_ = false;
    bool updateStarted_ = false;
    bool uploadFailed_ = false;
    bool lastSuccess_ = false;
    bool rebootPending_ = false;

    int responseStatus_ = 400;
    std::uint64_t successAtUs_ = 0;
    std::size_t receivedBytes_ = 0;

    std::array<std::uint8_t, 36> prefix_{};
    std::size_t prefixBytes_ = 0;

    std::array<char, 128> lastMessage_{};
    OtaUpdateStats stats_{};
};

} // namespace ambilight
