#include "transport/DdpAssembler.h"

#include <cstring>

namespace ambilight {

bool DdpAssembler::setFrameBytes(
    std::size_t frameBytes) {

    if (frameBytes == 0 ||
        frameBytes > kMaxFrameBytes ||
        frameBytes % sizeof(Rgb8) != 0) {

        return false;
    }

    if (frameBytes_ == frameBytes) {
        return true;
    }

    frameBytes_ = frameBytes;
    resetStream();
    return true;
}

void DdpAssembler::startFrame(
    std::uint8_t sequence,
    std::uint64_t nowUs) {

    coverage_.fill(0);
    coveredBytes_ = 0;
    contiguousPrefixBytes_ = 0;
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
    contiguousPrefixBytes_ = 0;
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
    if (packet.offset >= frameBytes_) {
        return false;
    }

    const std::uint64_t end =
        static_cast<std::uint64_t>(packet.offset) + packet.dataLength;

    return end <= frameBytes_;
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

void DdpAssembler::markUncoveredRange(
    std::size_t offset,
    std::size_t length) {

    std::size_t index = offset;
    const std::size_t end = offset + length;

    while (index < end &&
           (index & 7U) != 0U) {

        markCovered(index);
        ++index;
    }

    const std::size_t wholeBytes =
        (end - index) / 8U;

    if (wholeBytes > 0) {
        std::memset(
            coverage_.data() +
                index / 8U,
            0xFF,
            wholeBytes);

        index +=
            wholeBytes * 8U;
    }

    while (index < end) {
        markCovered(index);
        ++index;
    }
}

void DdpAssembler::refreshContiguousPrefix() {
    while (contiguousPrefixBytes_ <
               frameBytes_ &&
           isCovered(
               contiguousPrefixBytes_)) {

        ++contiguousPrefixBytes_;
    }
}

bool DdpAssembler::copySequentialFastPath(
    const DdpPacketView& packet) {

    const std::size_t offset =
        packet.offset;

    const std::size_t length =
        packet.dataLength;

    if (offset !=
            contiguousPrefixBytes_ ||
        coveredBytes_ !=
            contiguousPrefixBytes_) {

        return false;
    }

    std::memcpy(
        staging_.data() + offset,
        packet.payload,
        length);

    markUncoveredRange(
        offset,
        length);

    coveredBytes_ =
        static_cast<std::uint16_t>(
            coveredBytes_ +
            length);

    contiguousPrefixBytes_ =
        coveredBytes_;

    ++stats_.sequentialFastPathDatagrams;

    stats_.sequentialFastPathBytes +=
        length;

    return true;
}

bool DdpAssembler::copyAndMark(
    const DdpPacketView& packet) {

    if (copySequentialFastPath(
            packet)) {

        return true;
    }

    ++stats_.fallbackDatagrams;

    const std::size_t offset =
        packet.offset;

    const std::size_t length =
        packet.dataLength;

    // Fallback preserves the strict overlap semantics for duplicates,
    // reordering and conflicting fragments.
    for (std::size_t index = 0;
         index < length;
         ++index) {

        const std::size_t frameIndex =
            offset +
            index;

        if (isCovered(frameIndex) &&
            staging_[frameIndex] !=
                packet.payload[index]) {

            return false;
        }
    }

    for (std::size_t index = 0;
         index < length;
         ++index) {

        const std::size_t frameIndex =
            offset +
            index;

        if (isCovered(frameIndex)) {
            ++stats_.duplicateBytes;
        } else {
            markCovered(frameIndex);
            ++coveredBytes_;
        }

        staging_[frameIndex] =
            packet.payload[index];
    }

    refreshContiguousPrefix();

    return true;
}

bool DdpAssembler::isComplete() const {
    return pushSeen_ && coveredBytes_ == frameBytes_;
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

    DdpPacketView packet;

    if (parseDdpDatagram(
            datagram,
            datagramLength,
            packet) !=
        DdpParseError::None) {

        ++stats_.datagrams;
        ++stats_.rejected;

        expire(nowUs);

        return
            DdpIngestResult::Rejected;
    }

    return
        ingestParsed(
            packet,
            nowUs,
            completedFrame);
}

DdpIngestResult DdpAssembler::ingestParsed(
    const DdpPacketView& packet,
    std::uint64_t nowUs,
    RgbFrame& completedFrame) {

    ++stats_.datagrams;
    expire(nowUs);

    if (!payloadFits(packet)) {
        ++stats_.rejected;

        return
            DdpIngestResult::Rejected;
    }

    if (!active_) {
        if (!canStartSequence(
                packet.sequence,
                nowUs)) {

            ++stats_.stale;

            return
                DdpIngestResult::Stale;
        }

        startFrame(
            packet.sequence,
            nowUs);
    } else if (
        packet.sequence !=
            activeSequence_) {

        if (!ddpSequenceIsNewer(
                packet.sequence,
                activeSequence_)) {

            ++stats_.stale;

            return
                DdpIngestResult::Stale;
        }

        ++stats_.superseded;

        startFrame(
            packet.sequence,
            nowUs);
    }

    lastPacketUs_ =
        nowUs;

    if (!copyAndMark(packet)) {
        ++stats_.conflictingDatagrams;
        ++stats_.rejected;

        resetActive();

        return
            DdpIngestResult::Rejected;
    }

    pushSeen_ =
        pushSeen_ ||
        packet.push;

    if (!isComplete()) {
        ++stats_.partial;

        return
            DdpIngestResult::Partial;
    }

    std::memcpy(
        completedFrame.pixels.data(),
        staging_.data(),
        frameBytes_);

    completedFrame.pixelCount =
        static_cast<std::uint16_t>(
            frameBytes_ /
            sizeof(Rgb8));

    completedFrame.generation = 0;
    completedFrame.receivedUs =
        nowUs;

    ++stats_.completed;

    hasLastCompletedSequence_ =
        true;

    lastCompletedSequence_ =
        activeSequence_;

    lastCompletedUs_ =
        nowUs;

    resetActive();

    return
        DdpIngestResult::Complete;
}

} // namespace ambilight
