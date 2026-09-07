#include "network/DdpUdpService.h"

#include <cerrno>
#include <unistd.h>

#include <esp_timer.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>

namespace ambilight {

DdpUdpService::~DdpUdpService() {
    stop();
}

bool DdpUdpService::setLogicalLedCount(
    std::uint16_t logicalLedCount) {

    if (logicalLedCount == 0 ||
        logicalLedCount >
            config::kLogicalLedCapacity) {

        return false;
    }

    const std::size_t frameBytes =
        static_cast<std::size_t>(
            logicalLedCount) *
        sizeof(Rgb8);

    if (!assembler_.setFrameBytes(
            frameBytes) ||
        !senderGate_.
            setExpectedFrameBytes(
                frameBytes)) {

        return false;
    }

    logicalLedCount_ =
        logicalLedCount;

    // Topology may change lane assignment or direction while retaining the
    // same total. Every explicit topology apply starts a fresh sender/assembly
    // epoch so no transport state leaks across commissioning changes.
    senderGate_.reset();
    assembler_.resetStream();

    completedFrame_.clear();
    completedFrame_.pixelCount =
        logicalLedCount_;
    completedFrame_.generation = 0;
    publishedGeneration_ = 0;

    lastPacketUs_ = 0;
    lastCompleteFrameUs_ = 0;

    return true;
}

bool DdpUdpService::begin() {
    if (socket_ >= 0) {
        return true;
    }

    // Per-socket state must not leak across stop/reconfigure cycles.
    stats_.requestedRxBufferBytes =
        kRequestedSocketRxBufferBytes;
    stats_.actualRxBufferBytes = 0;
    stats_.rxBufferSetOk = false;
    stats_.rxBufferQueryOk = false;
    stats_.lastSocketOptionErrno = 0;

    socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ < 0) {
        stats_.lastSocketErrno = errno;
        ++stats_.socketErrors;
        return false;
    }

#ifdef SO_REUSEADDR
    const int reuse = 1;

    if (setsockopt(
            socket_,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuse,
            sizeof(reuse)) != 0) {

        ++stats_.socketOptionWarnings;
        ++stats_.reuseAddrSetFailures;
        stats_.lastSocketOptionErrno = errno;
    }
#endif

#ifdef SO_RCVBUF
    const int rxBufferBytes =
        kRequestedSocketRxBufferBytes;

    if (setsockopt(
            socket_,
            SOL_SOCKET,
            SO_RCVBUF,
            &rxBufferBytes,
            sizeof(rxBufferBytes)) == 0) {

        stats_.rxBufferSetOk = true;
    } else {
        ++stats_.socketOptionWarnings;
        ++stats_.rxBufferSetFailures;
        stats_.lastSocketOptionErrno = errno;
    }

    int actualRxBufferBytes = 0;
    socklen_t actualRxBufferLength =
        sizeof(actualRxBufferBytes);

    if (getsockopt(
            socket_,
            SOL_SOCKET,
            SO_RCVBUF,
            &actualRxBufferBytes,
            &actualRxBufferLength) == 0) {

        stats_.actualRxBufferBytes =
            actualRxBufferBytes;

        stats_.rxBufferQueryOk = true;
    } else {
        ++stats_.socketOptionWarnings;
        ++stats_.rxBufferQueryFailures;
        stats_.lastSocketOptionErrno = errno;
    }
#else
    ++stats_.socketOptionWarnings;
    ++stats_.rxBufferSetFailures;
    ++stats_.rxBufferQueryFailures;
#endif

    sockaddr_in localAddress{};
    localAddress.sin_family = AF_INET;
    localAddress.sin_port = htons(kPort);
    localAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(
            socket_,
            reinterpret_cast<const sockaddr*>(&localAddress),
            sizeof(localAddress)) != 0) {

        stats_.lastSocketErrno = errno;
        ++stats_.socketErrors;

        close(socket_);
        socket_ = -1;
        return false;
    }

    return true;
}

void DdpUdpService::stop() {
    if (socket_ >= 0) {
        close(socket_);
        socket_ = -1;
    }

    senderGate_.reset();
    assembler_.resetStream();
}

DdpPollResult DdpUdpService::poll() {
    DdpPollResult result;

    if (socket_ < 0) {
        return result;
    }

    const std::uint64_t pollStartedUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    assembler_.expire(pollStartedUs);

    if (senderGate_.expire(
            pollStartedUs)) {

        assembler_.resetStream();
    }

    bool haveCompletedFrame = false;

    while (result.datagrams < kMaxDatagramsPerPoll) {
        const std::uint64_t beforeReceiveUs =
            static_cast<std::uint64_t>(
                esp_timer_get_time());

        if (result.datagrams > 0 &&
            beforeReceiveUs - pollStartedUs >= kPollBudgetUs) {

            ++stats_.pollBudgetExhaustions;

            result.backlogLikely =
                result.acceptedDatagrams > 0 &&
                result.senderRejectedDatagrams == 0;

            break;
        }

        sockaddr_in sender{};
        socklen_t senderLength = sizeof(sender);

        const int received = recvfrom(
            socket_,
            rxBuffer_.data(),
            rxBuffer_.size(),
            MSG_DONTWAIT,
            reinterpret_cast<sockaddr*>(&sender),
            &senderLength);

        const std::uint64_t afterReceiveUs =
            static_cast<std::uint64_t>(
                esp_timer_get_time());

        stats_.receiveCallTime.observe(
            afterReceiveUs -
            beforeReceiveUs);

        if (received < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                result.socketDrained = true;
                break;
            }

            stats_.lastSocketErrno = errno;
            ++stats_.socketErrors;
            break;
        }

        ++result.datagrams;
        ++stats_.datagramsReceived;
        stats_.bytesReceived +=
            static_cast<std::uint64_t>(
                received);

        const std::uint64_t packetUs =
            afterReceiveUs;

        // A lease can expire while draining a busy socket. Reset the old
        // assembler epoch before allowing a new sender to acquire ownership.
        if (senderGate_.expire(
                packetUs)) {

            assembler_.resetStream();
        }

        const DdpSenderEndpoint endpoint{
            sender.sin_addr.s_addr,
            ntohs(sender.sin_port)
        };

        DdpPacketView packet;

        const std::uint64_t parseStartedUs =
            static_cast<std::uint64_t>(
                esp_timer_get_time());

        const DdpParseError parseResult =
            parseDdpDatagram(
                rxBuffer_.data(),
                static_cast<std::size_t>(
                    received),
                packet);

        stats_.parseTime.observe(
            static_cast<std::uint64_t>(
                esp_timer_get_time()) -
            parseStartedUs);

        if (parseResult !=
            DdpParseError::None) {

            ++stats_.parseFailures;
            ++result.senderRejectedDatagrams;
            continue;
        }

        const std::uint64_t senderGateStartedUs =
            static_cast<std::uint64_t>(
                esp_timer_get_time());

        const DdpSenderDecision senderDecision =
            senderGate_.evaluate(
                endpoint,
                packet,
                packetUs);

        stats_.senderGateTime.observe(
            static_cast<std::uint64_t>(
                esp_timer_get_time()) -
            senderGateStartedUs);

        if (senderDecision !=
            DdpSenderDecision::Accepted) {

            ++result.senderRejectedDatagrams;
            continue;
        }

        ++result.acceptedDatagrams;
        lastPacketUs_ = packetUs;

        const std::uint64_t assemblerStartedUs =
            static_cast<std::uint64_t>(
                esp_timer_get_time());

        const DdpIngestResult ingestResult =
            assembler_.ingestParsed(
                packet,
                packetUs,
                completedFrame_);

        stats_.assemblerTime.observe(
            static_cast<std::uint64_t>(
                esp_timer_get_time()) -
            assemblerStartedUs);

        if (ingestResult == DdpIngestResult::Complete) {
            ++result.completeFrames;
            ++stats_.completeFramesAssembled;
            haveCompletedFrame = true;
        }
    }

    if (result.datagrams == kMaxDatagramsPerPoll &&
        !result.socketDrained) {

        ++stats_.pollDatagramLimitHits;

        result.backlogLikely =
            result.acceptedDatagrams > 0 &&
            result.senderRejectedDatagrams == 0;
    }

    // Publish at most once per socket drain. completedFrame_ already contains
    // the newest assembled frame, so publication is an O(1) generation bump.
    // Renderer takes one stable snapshot copy later in the same loopTask.
    if (haveCompletedFrame) {
        ++publishedGeneration_;

        if (publishedGeneration_ == 0) {
            ++publishedGeneration_;
        }

        completedFrame_.generation =
            publishedGeneration_;

        result.framePublished = true;
        ++stats_.framePublications;

        if (result.completeFrames > 1) {
            stats_.collapsedCompleteFrames +=
                result.completeFrames - 1;
        }

        lastCompleteFrameUs_ =
            completedFrame_.receivedUs;
    }

    const std::uint64_t pollFinishedUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    result.elapsedUs =
        static_cast<std::uint32_t>(
            pollFinishedUs -
            pollStartedUs);

    stats_.pollTime.observe(
        result.elapsedUs);

    if (result.datagrams > stats_.maxDatagramsPerPoll) {
        stats_.maxDatagramsPerPoll = result.datagrams;
    }

    if (result.elapsedUs > stats_.maxPollUs) {
        stats_.maxPollUs = result.elapsedUs;
    }

    return result;
}

bool DdpUdpService::copyLatest(
    RgbFrame& destination,
    std::uint32_t lastGeneration) {

    if (publishedGeneration_ == 0 ||
        publishedGeneration_ ==
            lastGeneration) {

        return false;
    }

    const std::uint64_t startedUs =
        static_cast<std::uint64_t>(
            esp_timer_get_time());

    destination =
        completedFrame_;

    stats_.snapshotCopyTime.observe(
        static_cast<std::uint64_t>(
            esp_timer_get_time()) -
        startedUs);

    return true;
}

} // namespace ambilight
