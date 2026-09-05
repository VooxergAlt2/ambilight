#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/FrameMailbox.h"
#include "core/RgbFrame.h"
#include "transport/AwaParser.h"

namespace ambilight {

struct UsbAwaPollResult {
    std::uint32_t bytes = 0;
    std::uint32_t completeFrames = 0;
    std::uint32_t elapsedUs = 0;

    bool inputDrained = false;
    bool backlogLikely = false;
    bool mailboxPublished = false;
};

struct UsbAwaStats {
    std::uint64_t bytesReceived = 0;
    std::uint32_t completeFramesAssembled = 0;
    std::uint32_t mailboxPublications = 0;
    std::uint32_t collapsedCompleteFrames = 0;
    std::uint32_t handshakeReplies = 0;
    std::uint32_t handshakeReplyFailures = 0;
    std::uint32_t sleepRequests = 0;
    std::uint32_t pollBudgetExhaustions = 0;
    std::uint32_t readLimitHits = 0;
    std::uint32_t maxBytesPerPoll = 0;
    std::uint32_t maxPollUs = 0;
    std::uint32_t driverInstallErrors = 0;
};

class UsbAwaService {
public:
    static constexpr std::size_t kRxChunkSize = 2048;
    static constexpr std::size_t kMaxReadsPerPoll = 8;
    static constexpr std::uint32_t kPollBudgetUs = 3000;

    explicit UsbAwaService(FrameMailbox& mailbox)
        : mailbox_(mailbox) {}

    ~UsbAwaService();

    UsbAwaService(const UsbAwaService&) = delete;
    UsbAwaService& operator=(const UsbAwaService&) = delete;

    bool begin();
    void stop();

    UsbAwaPollResult poll();

    bool running() const { return ownsDriver_; }

    const UsbAwaStats& stats() const { return stats_; }
    const AwaParserStats& parserStats() const {
        return parser_.stats();
    }

    std::uint64_t lastFrameUs() const { return lastFrameUs_; }

private:
    bool sendHandshakeReply();

    FrameMailbox& mailbox_;
    AwaParser parser_;

    std::array<std::uint8_t, kRxChunkSize> rxChunk_{};
    RgbFrame completedFrame_{};

    bool ownsDriver_ = false;

    std::uint64_t lastFrameUs_ = 0;

    UsbAwaStats stats_{};
};

} // namespace ambilight
