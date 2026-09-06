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

    completedFrame_.clear();
    completedFrame_.pixelCount =
        logicalLedCount_;

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
        static_cast<std::uint64_t>(esp_timer_get_time());

    assembler_.expire(pollStartedUs);

    if (senderGate_.expire(
            pollStartedUs)) {

        assembler_.resetStream();
    }

    bool haveCompletedFrame = false;

    while (result.datagrams < kMaxDatagramsPerPoll) {
        const std::uint64_t beforeReceiveUs =
            static_cast<std::uint64_t>(esp_timer_get_time());

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
        stats_.bytesReceived += static_cast<std::uint64_t>(received);

        const std::uint64_t packetUs =
            static_cast<std::uint64_t>(esp_timer_get_time());

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

        const DdpSenderDecision senderDecision =
            senderGate_.evaluate(
                endpoint,
                rxBuffer_.data(),
                static_cast<std::size_t>(
                    received),
                packetUs);

        if (senderDecision !=
            DdpSenderDecision::Accepted) {

            ++result.senderRejectedDatagrams;
            continue;
        }

        ++result.acceptedDatagrams;
        lastPacketUs_ = packetUs;

        const DdpIngestResult ingestResult = assembler_.ingest(
            rxBuffer_.data(),
            static_cast<std::size_t>(received),
            packetUs,
            completedFrame_);

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

    // Publish at most once per socket drain. If several complete frames were
    // assembled, only the newest one survives. This collapses backlog before
    // taking the mailbox mutex and before paying PARLIO render cost.
    if (haveCompletedFrame) {
        if (mailbox_.publish(completedFrame_)) {
            result.mailboxPublished = true;
            ++stats_.mailboxPublications;

            if (result.completeFrames > 1) {
                stats_.collapsedCompleteFrames +=
                    result.completeFrames - 1;
            }

            lastCompleteFrameUs_ = completedFrame_.receivedUs;
        } else {
            ++stats_.publishFailures;
        }
    }

    const std::uint64_t pollFinishedUs =
        static_cast<std::uint64_t>(esp_timer_get_time());

    result.elapsedUs = static_cast<std::uint32_t>(
        pollFinishedUs - pollStartedUs);

    if (result.datagrams > stats_.maxDatagramsPerPoll) {
        stats_.maxDatagramsPerPoll = result.datagrams;
    }

    if (result.elapsedUs > stats_.maxPollUs) {
        stats_.maxPollUs = result.elapsedUs;
    }

    return result;
}

} // namespace ambilight
