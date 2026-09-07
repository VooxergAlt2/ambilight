#include <cstdint>
#include <vector>

#include <unity.h>

#include "network/DdpSenderGate.h"
#include "transport/DdpProtocol.h"

using ambilight::DdpSenderDecision;
using ambilight::DdpSenderEndpoint;
using ambilight::DdpSenderGate;

namespace {

std::vector<std::uint8_t> makePacket(
    std::uint8_t sequence = 1,
    std::uint32_t offset = 0,
    std::uint16_t payloadLength = 100,
    bool push = false) {

    std::vector<std::uint8_t> packet(
        ambilight::kDdpHeaderSize +
            payloadLength,
        0);

    packet[0] =
        static_cast<std::uint8_t>(
            ambilight::kDdpVersion1 |
            (push
                ? ambilight::kDdpPushFlag
                : 0));

    packet[1] = sequence;
    packet[2] =
        ambilight::kDdpRgbDataType;
    packet[3] =
        ambilight::kDdpDestination;

    packet[4] =
        static_cast<std::uint8_t>(
            (offset >> 24) & 0xFF);
    packet[5] =
        static_cast<std::uint8_t>(
            (offset >> 16) & 0xFF);
    packet[6] =
        static_cast<std::uint8_t>(
            (offset >> 8) & 0xFF);
    packet[7] =
        static_cast<std::uint8_t>(
            offset & 0xFF);

    packet[8] =
        static_cast<std::uint8_t>(
            (payloadLength >> 8) &
            0xFF);
    packet[9] =
        static_cast<std::uint8_t>(
            payloadLength & 0xFF);

    return packet;
}

constexpr DdpSenderEndpoint kSenderA{
    0x0100007FU,
    50000
};

constexpr DdpSenderEndpoint kSenderB{
    0x0200007FU,
    50001
};

ambilight::DdpPacketView parsePacket(
    const std::vector<std::uint8_t>& packet) {

    ambilight::DdpPacketView view;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            ambilight::DdpParseError::None),
        static_cast<int>(
            ambilight::parseDdpDatagram(
                packet.data(),
                packet.size(),
                view)));

    return view;
}

} // namespace

void test_invalid_packet_view_does_not_acquire_lock() {
    DdpSenderGate gate;

    ambilight::DdpPacketView invalid;
    invalid.offset = 0;
    invalid.dataLength = 100;
    invalid.payload = nullptr;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            DdpSenderDecision::InvalidDatagram),
        static_cast<int>(
            gate.evaluate(
                kSenderA,
                invalid,
                1000)));

    TEST_ASSERT_FALSE(
        gate.active());

    TEST_ASSERT_EQUAL_UINT32(
        0,
        gate.stats().lockAcquisitions);
}

void test_first_structurally_valid_packet_acquires_sender() {
    DdpSenderGate gate;
    const auto packet =
        makePacket();

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            DdpSenderDecision::Accepted),
        static_cast<int>(
            gate.evaluate(
                kSenderA,
                parsePacket(packet),
                1000)));

    TEST_ASSERT_TRUE(
        gate.active());

    TEST_ASSERT_TRUE(
        gate.activeSender() ==
            kSenderA);

    TEST_ASSERT_EQUAL_UINT64(
        1000,
        gate.lastAcceptedUs());

    TEST_ASSERT_EQUAL_UINT32(
        1,
        gate.stats().lockAcquisitions);
}

void test_same_sender_extends_lease() {
    DdpSenderGate gate(1000);
    const auto packet =
        makePacket();

    gate.evaluate(
        kSenderA,
        parsePacket(packet),
        1000);

    gate.evaluate(
        kSenderA,
        parsePacket(packet),
        1800);

    TEST_ASSERT_FALSE(
        gate.expire(2800));

    TEST_ASSERT_TRUE(
        gate.active());

    TEST_ASSERT_TRUE(
        gate.expire(2801));

    TEST_ASSERT_FALSE(
        gate.active());
}

void test_foreign_sender_is_dropped_without_extending_lease() {
    DdpSenderGate gate(1000);
    const auto packet =
        makePacket();

    gate.evaluate(
        kSenderA,
        parsePacket(packet),
        1000);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            DdpSenderDecision::ForeignSender),
        static_cast<int>(
            gate.evaluate(
                kSenderB,
                parsePacket(packet),
                1900)));

    TEST_ASSERT_EQUAL_UINT64(
        1000,
        gate.lastAcceptedUs());

    TEST_ASSERT_TRUE(
        gate.expire(2001));

    TEST_ASSERT_EQUAL_UINT32(
        1,
        gate.stats().foreignSenderDrops);
}

void test_invalid_packet_view_from_owner_does_not_extend_lease() {
    DdpSenderGate gate(1000);

    const auto valid =
        makePacket();

    gate.evaluate(
        kSenderA,
        parsePacket(valid),
        1000);

    ambilight::DdpPacketView invalid;
    invalid.offset = 0;
    invalid.dataLength = 100;
    invalid.payload = nullptr;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            DdpSenderDecision::InvalidDatagram),
        static_cast<int>(
            gate.evaluate(
                kSenderA,
                invalid,
                1900)));

    TEST_ASSERT_EQUAL_UINT64(
        1000,
        gate.lastAcceptedUs());

    TEST_ASSERT_TRUE(
        gate.expire(2001));
}

void test_out_of_frame_payload_does_not_acquire_lock() {
    DdpSenderGate gate;

    const auto packet =
        makePacket(
            1,
            2300,
            100,
            true);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            DdpSenderDecision::InvalidDatagram),
        static_cast<int>(
            gate.evaluate(
                kSenderA,
                parsePacket(packet),
                1000)));

    TEST_ASSERT_FALSE(
        gate.active());
}

void test_runtime_frame_bound_changes_sender_validation() {
    DdpSenderGate gate;

    TEST_ASSERT_TRUE(
        gate.setExpectedFrameBytes(
            900));

    const auto inside =
        makePacket(
            1,
            800,
            100,
            true);

    const auto outside =
        makePacket(
            1,
            850,
            100,
            true);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            DdpSenderDecision::Accepted),
        static_cast<int>(
            gate.evaluate(
                kSenderA,
                parsePacket(inside),
                1000)));

    gate.reset();

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            DdpSenderDecision::InvalidDatagram),
        static_cast<int>(
            gate.evaluate(
                kSenderA,
                parsePacket(outside),
                1100)));
}

void test_same_ip_different_port_is_foreign_sender() {
    DdpSenderGate gate;

    const auto packet =
        makePacket();

    DdpSenderEndpoint otherPort =
        kSenderA;

    otherPort.port =
        static_cast<std::uint16_t>(
            kSenderA.port + 1);

    gate.evaluate(
        kSenderA,
        parsePacket(packet),
        1000);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            DdpSenderDecision::ForeignSender),
        static_cast<int>(
            gate.evaluate(
                otherPort,
                parsePacket(packet),
                1100)));
}

void test_new_sender_can_acquire_after_timeout() {
    DdpSenderGate gate(1000);
    const auto packet =
        makePacket();

    gate.evaluate(
        kSenderA,
        parsePacket(packet),
        1000);

    TEST_ASSERT_TRUE(
        gate.expire(2001));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            DdpSenderDecision::Accepted),
        static_cast<int>(
            gate.evaluate(
                kSenderB,
                parsePacket(packet),
                2002)));

    TEST_ASSERT_TRUE(
        gate.activeSender() ==
            kSenderB);

    TEST_ASSERT_EQUAL_UINT32(
        2,
        gate.stats().lockAcquisitions);

    TEST_ASSERT_EQUAL_UINT32(
        1,
        gate.stats().lockTimeoutReleases);
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_invalid_packet_view_does_not_acquire_lock);
    RUN_TEST(test_first_structurally_valid_packet_acquires_sender);
    RUN_TEST(test_same_sender_extends_lease);
    RUN_TEST(test_foreign_sender_is_dropped_without_extending_lease);
    RUN_TEST(test_invalid_packet_view_from_owner_does_not_extend_lease);
    RUN_TEST(test_out_of_frame_payload_does_not_acquire_lock);
    RUN_TEST(test_runtime_frame_bound_changes_sender_validation);
    RUN_TEST(test_same_ip_different_port_is_foreign_sender);
    RUN_TEST(test_new_sender_can_acquire_after_timeout);

    return UNITY_END();
}
