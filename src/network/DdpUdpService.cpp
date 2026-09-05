#include "network/DdpUdpService.h"

#include <cerrno>
#include <cstring>
#include <unistd.h>

#include <esp_timer.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>

namespace ambilight {
namespace {

constexpr int kRequestedSocketRxBuffer = 32768;

} // namespace

DdpUdpService::~DdpUdpService() {
    stop();
}

bool DdpUdpService::begin() {
    if (socket_ >= 0) {
        return true;
    }

    socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ < 0) {
        stats_.lastSocketErrno = errno;
        ++stats_.socketErrors;
        return false;
    }

#ifdef SO_REUSEADDR
    const int reuse = 1;
    setsockopt(
        socket_,
        SOL_SOCKET,
        SO_REUSEADDR,
        &reuse,
        sizeof(reuse));
#endif

#ifdef SO_RCVBUF
    const int rxBufferBytes = kRequestedSocketRxBuffer;
    setsockopt(
        socket_,
        SOL_SOCKET,
        SO_RCVBUF,
        &rxBufferBytes,
        sizeof(rxBufferBytes));
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
    if (socket_ < 0) {
        return;
    }

    close(socket_);
    socket_ = -1;
}

std::uint32_t DdpUdpService::poll() {
    if (socket_ < 0) {
        return 0;
    }

    assembler_.expire(
        static_cast<std::uint64_t>(esp_timer_get_time()));

    std::uint32_t drained = 0;
    std::uint32_t completedThisPoll = 0;

    while (drained < kMaxDatagramsPerPoll) {
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
                break;
            }

            stats_.lastSocketErrno = errno;
            ++stats_.socketErrors;
            break;
        }

        ++drained;
        ++stats_.datagramsReceived;
        stats_.bytesReceived += static_cast<std::uint64_t>(received);

        const std::uint64_t packetUs =
            static_cast<std::uint64_t>(esp_timer_get_time());

        lastPacketUs_ = packetUs;
        lastSenderIpv4_ = sender.sin_addr.s_addr;
        lastSenderPort_ = ntohs(sender.sin_port);

        const DdpIngestResult result = assembler_.ingest(
            rxBuffer_.data(),
            static_cast<std::size_t>(received),
            packetUs,
            completedFrame_);

        if (result != DdpIngestResult::Complete) {
            continue;
        }

        if (!mailbox_.publish(completedFrame_)) {
            ++stats_.publishFailures;
            continue;
        }

        ++stats_.completeFramesPublished;
        ++completedThisPoll;
        lastCompleteFrameUs_ = packetUs;
    }

    if (drained > stats_.maxDatagramsPerPoll) {
        stats_.maxDatagramsPerPoll = drained;
    }

    return completedThisPoll;
}

} // namespace ambilight
