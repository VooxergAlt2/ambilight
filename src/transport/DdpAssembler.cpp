#include "transport/DdpAssembler.h"

#include <cstring>

namespace ambilight {

void DdpAssembler::startFrame(
    std::uint8_t sequence,
    std::uint64_t nowUs) {

    coverage_.fill(0);
    coveredBytes_ = 0;
    pushSeen_ = false;

    active_ = true;
    activeSequence_ = sequence;
    lastPacketUs_ = nowUs;
}

void DdpAssembler::resetActive() {
    active_ = false;
    pushSeen_ = false;
    activeSequence_ = 0;
    coveredBytes_ = 0;
    lastPacketUs_ = 0;
    coverage_.fill(0);
}

bool DdpAssembler::expire(std::uint64_t nowUs) {
    if (!active_) {
        return false;
    }

    if (nowUs - lastPacketUs_ <= assemblyTimeoutUs_) {
        return false;
    }

    ++stats_.timedOut;
    resetActive();
    return true;
}

void DdpAssembler::resetStream() {
    resetActive();

    hasLastCompletedSequence_ = false;
    lastCompletedSequence_ = 0;
    lastCompletedUs_ = 0;
}

bool DdpAssembler::payloadFits(const DdpPacketView& packet) const {
    if (packet.offset >= kFrameBytes) {
        return false;
    }

    const std::uint64_t end =
        static_cast<std::uint64_t>(packet.offset) + packet.dataLength;

    return end <= kFrameBytes;
}

bool DdpAssembler::isCovered(std::size_t index) const {
    const std::size_t byte = index / 8;
    const std::uint8_t bit = static_cast<std::uint8_t>(index % 8);

    return (coverage_[byte] & static_cast<std::uint8_t>(1U << bit)) != 0;
}

void DdpAssembler::markCovered(std::size_t index) {
    const std::size_t byte = index / 8;
    const std::uint8_t bit = static_cast<std::uint8_t>(index % 8);

    coverage_[byte] |= static_cast<std::uint8_t>(1U << bit);
}

bool DdpAssembler::copyAndMark(const DdpPacketView& packet) {
    const std::size_t offset = packet.offset;
    const std::size_t length = packet.dataLength;

    // A duplicate or overlapping packet is acceptable only when bytes already
    // received for this sequence are identical. Conflicting overlap would make
    // frame contents order-dependent, so reject the active frame instead.
    for (std::size_t index = 0; index < length; ++index) {
        const std::size_t frameIndex = offset + index;

        if (isCovered(frameIndex) &&
            staging_[frameIndex] != packet.payload[index]) {
            return false;
        }
    }

    for (std::size_t index = 0; index < length; ++index) {
        const std::size_t frameIndex = offset + index;

        if (isCovered(frameIndex)) {
            ++stats_.duplicateBytes;
        } else {
            markCovered(frameIndex);
            ++coveredBytes_;
        }

        staging_[frameIndex] = packet.payload[index];
    }

    return true;
}

bool DdpAssembler::isComplete() const {
    return pushSeen_ && coveredBytes_ == kFrameBytes;
}

bool DdpAssembler::canStartSequence(
    std::uint8_t sequence,
    std::uint64_t nowUs) {

    if (!hasLastCompletedSequence_) {
        return true;
    }

    if (sequence == lastCompletedSequence_) {
        return false;
    }

    if (nowUs - lastCompletedUs_ > kSequenceResyncSilenceUs) {
        ++stats_.sequenceResyncs;
        return true;
    }

    return ddpSequenceIsNewer(sequence, lastCompletedSequence_);
}

DdpIngestResult DdpAssembler::ingest(
    const std::uint8_t* datagram,
    std::size_t datagramLength,
    std::uint64_t nowUs,
    RgbFrame& completedFrame) {

    ++stats_.datagrams;
    expire(nowUs);

    DdpPacketView packet;
    const DdpParseError parseResult =
        parseDdpDatagram(datagram, datagramLength, packet);

    if (parseResult != DdpParseError::None || !payloadFits(packet)) {
        ++stats_.rejected;
        return DdpIngestResult::Rejected;
    }

    if (!active_) {
        if (!canStartSequence(packet.sequence, nowUs)) {
            ++stats_.stale;
            return DdpIngestResult::Stale;
        }

        startFrame(packet.sequence, nowUs);
    } else if (packet.sequence != activeSequence_) {
        if (!ddpSequenceIsNewer(packet.sequence, activeSequence_)) {
            ++stats_.stale;
            return DdpIngestResult::Stale;
        }

        ++stats_.superseded;
        startFrame(packet.sequence, nowUs);
    }

    lastPacketUs_ = nowUs;

    if (!copyAndMark(packet)) {
        ++stats_.conflictingDatagrams;
        ++stats_.rejected;
        resetActive();
        return DdpIngestResult::Rejected;
    }

    pushSeen_ = pushSeen_ || packet.push;

    if (!isComplete()) {
        ++stats_.partial;
        return DdpIngestResult::Partial;
    }

    static_assert(sizeof(completedFrame.pixels) == kFrameBytes);
    std::memcpy(
        completedFrame.pixels.data(),
        staging_.data(),
        kFrameBytes);

    completedFrame.generation = 0;
    completedFrame.receivedUs = nowUs;

    ++stats_.completed;

    hasLastCompletedSequence_ = true;
    lastCompletedSequence_ = activeSequence_;
    lastCompletedUs_ = nowUs;

    resetActive();

    return DdpIngestResult::Complete;
}

} // namespace ambilight
