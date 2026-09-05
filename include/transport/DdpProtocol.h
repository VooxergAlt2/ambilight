#pragma once

#include <cstddef>
#include <cstdint>

namespace ambilight {

constexpr std::size_t kDdpHeaderSize = 10;
constexpr std::uint8_t kDdpVersion1Mask = 0xC0;
constexpr std::uint8_t kDdpVersion1 = 0x40;
constexpr std::uint8_t kDdpPushFlag = 0x01;
constexpr std::uint8_t kDdpRgbDataType = 0x0B;
constexpr std::uint8_t kDdpDestination = 0x01;

enum class DdpParseError : std::uint8_t {
    None = 0,
    TooShort,
    WrongVersion,
    InvalidSequence,
    UnsupportedDataType,
    WrongDestination,
    EmptyPayload,
    LengthMismatch
};

struct DdpPacketView {
    std::uint8_t flags = 0;
    std::uint8_t sequence = 0;
    std::uint8_t dataType = 0;
    std::uint8_t destination = 0;

    std::uint32_t offset = 0;
    std::uint16_t dataLength = 0;

    const std::uint8_t* payload = nullptr;
    bool push = false;
};

constexpr std::uint16_t readBe16(const std::uint8_t* bytes) {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(bytes[0]) << 8) |
        static_cast<std::uint16_t>(bytes[1]));
}

constexpr std::uint32_t readBe32(const std::uint8_t* bytes) {
    return
        (static_cast<std::uint32_t>(bytes[0]) << 24) |
        (static_cast<std::uint32_t>(bytes[1]) << 16) |
        (static_cast<std::uint32_t>(bytes[2]) << 8) |
        static_cast<std::uint32_t>(bytes[3]);
}

inline DdpParseError parseDdpDatagram(
    const std::uint8_t* data,
    std::size_t datagramLength,
    DdpPacketView& packet) {

    if (data == nullptr || datagramLength < kDdpHeaderSize) {
        return DdpParseError::TooShort;
    }

    packet.flags = data[0];
    packet.sequence = data[1];
    packet.dataType = data[2];
    packet.destination = data[3];
    packet.offset = readBe32(data + 4);
    packet.dataLength = readBe16(data + 8);
    packet.payload = data + kDdpHeaderSize;
    packet.push = (packet.flags & kDdpPushFlag) != 0;

    if ((packet.flags & kDdpVersion1Mask) != kDdpVersion1) {
        return DdpParseError::WrongVersion;
    }

    // HyperHDR 22 increments one frame sequence in the 1..15 range.
    if (packet.sequence == 0 || packet.sequence > 15) {
        return DdpParseError::InvalidSequence;
    }

    if (packet.dataType != kDdpRgbDataType) {
        return DdpParseError::UnsupportedDataType;
    }

    if (packet.destination != kDdpDestination) {
        return DdpParseError::WrongDestination;
    }

    if (packet.dataLength == 0) {
        return DdpParseError::EmptyPayload;
    }

    if (datagramLength != kDdpHeaderSize + packet.dataLength) {
        return DdpParseError::LengthMismatch;
    }

    return DdpParseError::None;
}

// HyperHDR uses a 1..15 ring. This comparison deliberately assumes normal
// realtime ordering, where a newly observed frame is at most seven sequence
// steps ahead. Older/later reordered frames are therefore rejected instead
// of being allowed to rewind the active assembler.
constexpr bool ddpSequenceIsNewer(
    std::uint8_t candidate,
    std::uint8_t reference) {

    if (candidate == 0 || candidate > 15 ||
        reference == 0 || reference > 15 ||
        candidate == reference) {
        return false;
    }

    const std::uint8_t forward = static_cast<std::uint8_t>(
        (candidate + 15 - reference) % 15);

    return forward > 0 && forward <= 7;
}

} // namespace ambilight
