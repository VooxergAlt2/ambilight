#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/RgbFrame.h"
#include "transport/DdpProtocol.h"

namespace ambilight {

enum class DdpIngestResult : std::uint8_t {
    Rejected = 0,
    Stale,
    Partial,
    Complete
};

struct DdpAssemblerStats {
    std::uint32_t datagrams = 0;
    std::uint32_t rejected = 0;
    std::uint32_t stale = 0;
    std::uint32_t partial = 0;
    std::uint32_t completed = 0;
    std::uint32_t superseded = 0;
    std::uint32_t timedOut = 0;
    std::uint32_t sequenceResyncs = 0;
    std::uint32_t duplicateBytes = 0;
    std::uint32_t conflictingDatagrams = 0;
};

class DdpAssembler {
public:
    static constexpr std::size_t kDefaultFrameBytes =
        config::kDefaultLogicalLedCount * sizeof(Rgb8);

    static constexpr std::size_t kMaxFrameBytes =
        config::kLogicalLedCapacity * sizeof(Rgb8);

    static constexpr std::size_t kCoverageBytes =
        (kMaxFrameBytes + 7) / 8;
    static constexpr std::uint64_t kDefaultAssemblyTimeoutUs = 50000;
    static constexpr std::uint64_t kSequenceResyncSilenceUs = 250000;

    explicit DdpAssembler(
        std::uint64_t assemblyTimeoutUs = kDefaultAssemblyTimeoutUs)
        : assemblyTimeoutUs_(assemblyTimeoutUs) {}

    bool setFrameBytes(
        std::size_t frameBytes);

    std::size_t frameBytes() const {
        return frameBytes_;
    }

    DdpIngestResult ingest(
        const std::uint8_t* datagram,
        std::size_t datagramLength,
        std::uint64_t nowUs,
        RgbFrame& completedFrame);

    bool expire(std::uint64_t nowUs);

    // Start a new sender/transport epoch without erasing accumulated stats.
    // This clears both partial-frame state and completed-sequence history.
    void resetStream();

    const DdpAssemblerStats& stats() const { return stats_; }

    bool active() const { return active_; }
    std::uint8_t activeSequence() const { return activeSequence_; }
    std::uint16_t coveredBytes() const { return coveredBytes_; }

private:
    void startFrame(std::uint8_t sequence, std::uint64_t nowUs);
    void resetActive();
    bool payloadFits(const DdpPacketView& packet) const;
    bool copyAndMark(const DdpPacketView& packet);
    bool isCovered(std::size_t index) const;
    void markCovered(std::size_t index);
    bool isComplete() const;
    bool canStartSequence(std::uint8_t sequence, std::uint64_t nowUs);

    std::array<std::uint8_t, kMaxFrameBytes> staging_{};
    std::array<std::uint8_t, kCoverageBytes> coverage_{};

    bool active_ = false;
    bool pushSeen_ = false;

    bool hasLastCompletedSequence_ = false;
    std::uint8_t activeSequence_ = 0;
    std::uint8_t lastCompletedSequence_ = 0;

    std::uint16_t coveredBytes_ = 0;

    std::uint64_t lastPacketUs_ = 0;
    std::uint64_t lastCompletedUs_ = 0;
    std::uint64_t assemblyTimeoutUs_ = kDefaultAssemblyTimeoutUs;

    std::size_t frameBytes_ =
        kDefaultFrameBytes;

    DdpAssemblerStats stats_{};
};

static_assert(DdpAssembler::kDefaultFrameBytes == 2340);
static_assert(DdpAssembler::kMaxFrameBytes == 2760);
static_assert(DdpAssembler::kCoverageBytes == 345);

} // namespace ambilight
