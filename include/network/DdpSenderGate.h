#pragma once

#include <cstddef>
#include <cstdint>

#include "core/RgbFrame.h"
#include "transport/DdpProtocol.h"

namespace ambilight {

struct DdpSenderEndpoint {
    std::uint32_t ipv4NetworkOrder = 0;
    std::uint16_t port = 0;

    constexpr bool operator==(
        const DdpSenderEndpoint& other) const {

        return
            ipv4NetworkOrder ==
                other.ipv4NetworkOrder &&
            port == other.port;
    }

    constexpr bool operator!=(
        const DdpSenderEndpoint& other) const {

        return !(*this == other);
    }
};

enum class DdpSenderDecision : std::uint8_t {
    Accepted = 0,
    InvalidDatagram,
    ForeignSender
};

struct DdpSenderGateStats {
    std::uint32_t acceptedDatagrams = 0;
    std::uint32_t invalidDatagrams = 0;
    std::uint32_t foreignSenderDrops = 0;

    std::uint32_t lockAcquisitions = 0;
    std::uint32_t lockTimeoutReleases = 0;
    std::uint32_t explicitResets = 0;
};

class DdpSenderGate {
public:
    static constexpr std::size_t kDefaultExpectedFrameBytes =
        config::kDefaultLogicalLedCount *
        sizeof(Rgb8);

    static constexpr std::size_t kMaxExpectedFrameBytes =
        config::kLogicalLedCapacity *
        sizeof(Rgb8);

    // One second is long relative to a realtime DDP stream but short enough
    // that a deliberately stopped sender can hand control to another PC
    // without a reboot.
    static constexpr std::uint64_t kDefaultLeaseTimeoutUs =
        1000000ULL;

    explicit DdpSenderGate(
        std::uint64_t leaseTimeoutUs =
            kDefaultLeaseTimeoutUs)
        : leaseTimeoutUs_(
              leaseTimeoutUs) {}

    bool setExpectedFrameBytes(
        std::size_t frameBytes) {

        if (frameBytes == 0 ||
            frameBytes >
                kMaxExpectedFrameBytes ||
            frameBytes %
                sizeof(Rgb8) != 0) {

            return false;
        }

        if (expectedFrameBytes_ ==
            frameBytes) {

            return true;
        }

        expectedFrameBytes_ =
            frameBytes;

        reset();
        return true;
    }

    std::size_t expectedFrameBytes() const {
        return expectedFrameBytes_;
    }

    DdpSenderDecision evaluate(
        const DdpSenderEndpoint& sender,
        const std::uint8_t* datagram,
        std::size_t datagramLength,
        std::uint64_t nowUs) {

        if (!datagramStructurallyValid(
                datagram,
                datagramLength)) {

            ++stats_.invalidDatagrams;
            return
                DdpSenderDecision::InvalidDatagram;
        }

        if (!active_) {
            active_ = true;
            activeSender_ = sender;
            lastAcceptedUs_ = nowUs;

            ++stats_.acceptedDatagrams;
            ++stats_.lockAcquisitions;

            return
                DdpSenderDecision::Accepted;
        }

        if (sender != activeSender_) {
            ++stats_.foreignSenderDrops;
            return
                DdpSenderDecision::ForeignSender;
        }

        lastAcceptedUs_ = nowUs;
        ++stats_.acceptedDatagrams;

        return
            DdpSenderDecision::Accepted;
    }

    bool expire(std::uint64_t nowUs) {
        if (!active_) {
            return false;
        }

        if (nowUs < lastAcceptedUs_) {
            return false;
        }

        if (nowUs - lastAcceptedUs_ <=
            leaseTimeoutUs_) {

            return false;
        }

        clearActive();
        ++stats_.lockTimeoutReleases;
        return true;
    }

    void reset() {
        if (active_) {
            ++stats_.explicitResets;
        }

        clearActive();
    }

    bool active() const {
        return active_;
    }

    const DdpSenderEndpoint& activeSender() const {
        return activeSender_;
    }

    std::uint64_t lastAcceptedUs() const {
        return lastAcceptedUs_;
    }

    std::uint64_t leaseTimeoutUs() const {
        return leaseTimeoutUs_;
    }

    const DdpSenderGateStats& stats() const {
        return stats_;
    }

private:
    bool datagramStructurallyValid(
        const std::uint8_t* datagram,
        std::size_t datagramLength) {

        DdpPacketView packet;

        if (parseDdpDatagram(
                datagram,
                datagramLength,
                packet) !=
            DdpParseError::None) {

            return false;
        }

        if (packet.offset >=
            expectedFrameBytes_) {

            return false;
        }

        const std::uint64_t end =
            static_cast<std::uint64_t>(
                packet.offset) +
            static_cast<std::uint64_t>(
                packet.dataLength);

        return end <=
            expectedFrameBytes_;
    }

    void clearActive() {
        active_ = false;
        activeSender_ = {};
        lastAcceptedUs_ = 0;
    }

    std::uint64_t leaseTimeoutUs_ =
        kDefaultLeaseTimeoutUs;

    std::size_t expectedFrameBytes_ =
        kDefaultExpectedFrameBytes;

    bool active_ = false;

    DdpSenderEndpoint activeSender_{};
    std::uint64_t lastAcceptedUs_ = 0;

    DdpSenderGateStats stats_{};
};

static_assert(
    DdpSenderGate::kDefaultExpectedFrameBytes == 2340);

static_assert(
    DdpSenderGate::kMaxExpectedFrameBytes == 2760);

} // namespace ambilight
