#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/PerformanceMetric.h"
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
    bool framePublished = false;
};

struct DdpUdpStats {
    std::uint32_t datagramsReceived = 0;
    std::uint64_t bytesReceived = 0;

    std::uint32_t completeFramesAssembled = 0;
    std::uint32_t framePublications = 0;
    std::uint32_t collapsedCompleteFrames = 0;

    std::uint32_t socketErrors = 0;
    std::uint32_t parseFailures = 0;

    std::uint32_t socketOptionWarnings = 0;
    std::uint32_t reuseAddrSetFailures = 0;
    std::uint32_t rxBufferSetFailures = 0;
    std::uint32_t rxBufferQueryFailures = 0;

    std::int32_t requestedRxBufferBytes = 0;
    std::int32_t actualRxBufferBytes = 0;

    std::uint32_t pollBudgetExhaustions = 0;
    std::uint32_t pollDatagramLimitHits = 0;

    std::uint32_t maxDatagramsPerPoll = 0;
    std::uint32_t maxPollUs = 0;

    int lastSocketErrno = 0;
    int lastSocketOptionErrno = 0;

    bool rxBufferSetOk = false;
    bool rxBufferQueryOk = false;

    PerformanceMetric pollTime{};
    PerformanceMetric receiveCallTime{};
    PerformanceMetric parseTime{};
    PerformanceMetric senderGateTime{};
    PerformanceMetric assemblerTime{};
    PerformanceMetric snapshotCopyTime{};
};

class DdpUdpService {
public:
    static constexpr std::uint16_t kPort = 4048;
    static constexpr std::size_t kRxBufferSize = 1536;
    static constexpr int kRequestedSocketRxBufferBytes = 32768;

    // A temporary backlog is more efficiently collapsed by parsing packets
    // than by rendering old frames. Both a packet cap and a time budget keep
    // the single C6 core bounded.
    static constexpr std::size_t kMaxDatagramsPerPoll = 128;
    static constexpr std::uint32_t kPollBudgetUs = 3000;

    DdpUdpService() = default;
    ~DdpUdpService();

    DdpUdpService(const DdpUdpService&) = delete;
    DdpUdpService& operator=(const DdpUdpService&) = delete;

    bool begin();
    void stop();

    bool setLogicalLedCount(
        std::size_t logicalLedCount);

    std::size_t logicalLedCount() const {
        return logicalLedCount_;
    }

    DdpPollResult poll();

    // Single-loop-task latest-frame snapshot. DDP owns the mutable assembled
    // frame; renderer receives one stable copy after poll() completes.
    bool copyLatest(
        RgbFrame& destination,
        std::uint32_t lastGeneration);

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
    DdpAssembler assembler_;
    DdpSenderGate senderGate_;

    int socket_ = -1;

    std::array<std::uint8_t, kRxBufferSize> rxBuffer_{};
    RgbFrame completedFrame_{};
    std::uint32_t publishedGeneration_ = 0;

    DdpUdpStats stats_{};

    std::size_t logicalLedCount_ =
        config::kDefaultLogicalLedCount;

    std::uint64_t lastPacketUs_ = 0;
    std::uint64_t lastCompleteFrameUs_ = 0;
};

static_assert(
    DdpUdpService::kRxBufferSize >= 1450,
    "HyperHDR DDP RGB packet requires at least 1450 bytes");

} // namespace ambilight
