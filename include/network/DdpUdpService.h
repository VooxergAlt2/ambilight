#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/FrameMailbox.h"
#include "core/RgbFrame.h"
#include "network/DdpSenderGate.h"
#include "transport/DdpAssembler.h"

namespace ambilight {

struct DdpPollResult {
    std::uint32_t datagrams = 0;
    std::uint32_t acceptedDatagrams = 0;
    std::uint32_t senderRejectedDatagrams = 0;

    std::uint32_t completeFrames = 0;
    std::uint32_t elapsedUs = 0;

    bool socketDrained = false;
    bool backlogLikely = false;
    bool mailboxPublished = false;
};

struct DdpUdpStats {
    std::uint32_t datagramsReceived = 0;
    std::uint64_t bytesReceived = 0;

    std::uint32_t completeFramesAssembled = 0;
    std::uint32_t mailboxPublications = 0;
    std::uint32_t collapsedCompleteFrames = 0;
    std::uint32_t publishFailures = 0;

    std::uint32_t socketErrors = 0;
    std::uint32_t pollBudgetExhaustions = 0;
    std::uint32_t pollDatagramLimitHits = 0;

    std::uint32_t maxDatagramsPerPoll = 0;
    std::uint32_t maxPollUs = 0;

    int lastSocketErrno = 0;
};

class DdpUdpService {
public:
    static constexpr std::uint16_t kPort = 4048;
    static constexpr std::size_t kRxBufferSize = 1536;

    // A temporary backlog is more efficiently collapsed by parsing packets
    // than by rendering old frames. Both a packet cap and a time budget keep
    // the single C6 core bounded.
    static constexpr std::size_t kMaxDatagramsPerPoll = 128;
    static constexpr std::uint32_t kPollBudgetUs = 3000;

    explicit DdpUdpService(FrameMailbox& mailbox)
        : mailbox_(mailbox) {}

    ~DdpUdpService();

    DdpUdpService(const DdpUdpService&) = delete;
    DdpUdpService& operator=(const DdpUdpService&) = delete;

    bool begin();
    void stop();

    DdpPollResult poll();

    bool running() const { return socket_ >= 0; }

    const DdpUdpStats& stats() const { return stats_; }
    const DdpAssemblerStats& assemblerStats() const {
        return assembler_.stats();
    }

    const DdpSenderGateStats& senderGateStats() const {
        return senderGate_.stats();
    }

    bool senderLocked() const {
        return senderGate_.active();
    }

    std::uint32_t activeSenderIpv4NetworkOrder() const {
        return senderGate_.active()
            ? senderGate_.activeSender().ipv4NetworkOrder
            : 0;
    }

    std::uint16_t activeSenderPort() const {
        return senderGate_.active()
            ? senderGate_.activeSender().port
            : 0;
    }

    std::uint64_t lastPacketUs() const { return lastPacketUs_; }
    std::uint64_t lastCompleteFrameUs() const {
        return lastCompleteFrameUs_;
    }

private:
    FrameMailbox& mailbox_;
    DdpAssembler assembler_;
    DdpSenderGate senderGate_;

    int socket_ = -1;

    std::array<std::uint8_t, kRxBufferSize> rxBuffer_{};
    RgbFrame completedFrame_{};

    DdpUdpStats stats_{};

    std::uint64_t lastPacketUs_ = 0;
    std::uint64_t lastCompleteFrameUs_ = 0;
};

static_assert(
    DdpUdpService::kRxBufferSize >= 1450,
    "HyperHDR DDP RGB packet requires at least 1450 bytes");

} // namespace ambilight
