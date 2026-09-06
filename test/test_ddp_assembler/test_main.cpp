#include <cstdint>
#include <vector>

#include <unity.h>

#include "transport/DdpAssembler.h"
#include "transport/DdpProtocol.h"

using ambilight::DdpAssembler;
using ambilight::DdpIngestResult;
using ambilight::RgbFrame;

namespace {

std::vector<std::uint8_t> makePacket(
    std::uint8_t sequence,
    std::uint32_t offset,
    std::uint16_t payloadLength,
    bool push,
    std::uint8_t dataType = ambilight::kDdpRgbDataType,
    std::uint8_t destination = ambilight::kDdpDestination) {

    std::vector<std::uint8_t> packet(
        ambilight::kDdpHeaderSize + payloadLength,
        0);

    packet[0] = static_cast<std::uint8_t>(
        ambilight::kDdpVersion1 |
        (push ? ambilight::kDdpPushFlag : 0));
    packet[1] = sequence;
    packet[2] = dataType;
    packet[3] = destination;

    packet[4] = static_cast<std::uint8_t>((offset >> 24) & 0xFF);
    packet[5] = static_cast<std::uint8_t>((offset >> 16) & 0xFF);
    packet[6] = static_cast<std::uint8_t>((offset >> 8) & 0xFF);
    packet[7] = static_cast<std::uint8_t>(offset & 0xFF);

    packet[8] = static_cast<std::uint8_t>((payloadLength >> 8) & 0xFF);
    packet[9] = static_cast<std::uint8_t>(payloadLength & 0xFF);

    for (std::uint16_t index = 0; index < payloadLength; ++index) {
        packet[ambilight::kDdpHeaderSize + index] =
            static_cast<std::uint8_t>((offset + index) & 0xFF);
    }

    return packet;
}

void assertFramePattern(const RgbFrame& frame) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(
        frame.pixels.data());

    for (std::size_t index = 0; index < DdpAssembler::kDefaultFrameBytes; ++index) {
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<std::uint8_t>(index & 0xFF),
            bytes[index]);
    }
}

} // namespace

void test_hyperhdr_two_packet_frame_completes() {
    DdpAssembler assembler;
    RgbFrame frame;

    auto first = makePacket(1, 0, 1440, false);
    auto second = makePacket(1, 1440, 900, true);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DdpIngestResult::Partial),
        static_cast<int>(assembler.ingest(
            first.data(), first.size(), 1000, frame)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DdpIngestResult::Complete),
        static_cast<int>(assembler.ingest(
            second.data(), second.size(), 1200, frame)));

    assertFramePattern(frame);
    TEST_ASSERT_EQUAL_UINT32(1, assembler.stats().completed);
    TEST_ASSERT_EQUAL_UINT16(0, assembler.coveredBytes());
}

void test_push_packet_can_arrive_first() {
    DdpAssembler assembler;
    RgbFrame frame;

    auto first = makePacket(2, 0, 1440, false);
    auto second = makePacket(2, 1440, 900, true);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DdpIngestResult::Partial),
        static_cast<int>(assembler.ingest(
            second.data(), second.size(), 2000, frame)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DdpIngestResult::Complete),
        static_cast<int>(assembler.ingest(
            first.data(), first.size(), 2200, frame)));

    assertFramePattern(frame);
}

void test_duplicate_packet_does_not_fake_completion() {
    DdpAssembler assembler;
    RgbFrame frame;

    auto first = makePacket(3, 0, 1440, false);
    auto second = makePacket(3, 1440, 900, true);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DdpIngestResult::Partial),
        static_cast<int>(assembler.ingest(
            first.data(), first.size(), 3000, frame)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DdpIngestResult::Partial),
        static_cast<int>(assembler.ingest(
            first.data(), first.size(), 3100, frame)));

    TEST_ASSERT_EQUAL_UINT16(1440, assembler.coveredBytes());

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DdpIngestResult::Complete),
        static_cast<int>(assembler.ingest(
            second.data(), second.size(), 3200, frame)));

    TEST_ASSERT_EQUAL_UINT32(1440, assembler.stats().duplicateBytes);
}

void test_new_sequence_supersedes_incomplete_frame() {
    DdpAssembler assembler;
    RgbFrame frame;

    auto oldFirst = makePacket(4, 0, 1440, false);
    auto newFirst = makePacket(5, 0, 1440, false);
    auto newSecond = makePacket(5, 1440, 900, true);

    assembler.ingest(oldFirst.data(), oldFirst.size(), 4000, frame);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DdpIngestResult::Partial),
        static_cast<int>(assembler.ingest(
            newFirst.data(), newFirst.size(), 4100, frame)));

    TEST_ASSERT_EQUAL_UINT32(1, assembler.stats().superseded);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DdpIngestResult::Complete),
        static_cast<int>(assembler.ingest(
            newSecond.data(), newSecond.size(), 4200, frame)));
}

void test_stale_sequence_is_rejected_after_completion() {
    DdpAssembler assembler;
    RgbFrame frame;

    auto first = makePacket(6, 0, 1440, false);
    auto second = makePacket(6, 1440, 900, true);
    auto stale = makePacket(5, 0, 1440, false);

    assembler.ingest(first.data(), first.size(), 5000, frame);
    assembler.ingest(second.data(), second.size(), 5100, frame);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DdpIngestResult::Stale),
        static_cast<int>(assembler.ingest(
            stale.data(), stale.size(), 5200, frame)));
}

void test_sequence_wrap_15_to_1_is_newer() {
    TEST_ASSERT_TRUE(ambilight::ddpSequenceIsNewer(1, 15));

    DdpAssembler assembler;
    RgbFrame frame;

    auto a0 = makePacket(15, 0, 1440, false);
    auto a1 = makePacket(15, 1440, 900, true);
    auto b0 = makePacket(1, 0, 1440, false);
    auto b1 = makePacket(1, 1440, 900, true);

    assembler.ingest(a0.data(), a0.size(), 6000, frame);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DdpIngestResult::Complete),
        static_cast<int>(assembler.ingest(
            a1.data(), a1.size(), 6100, frame)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DdpIngestResult::Partial),
        static_cast<int>(assembler.ingest(
            b0.data(), b0.size(), 6200, frame)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DdpIngestResult::Complete),
        static_cast<int>(assembler.ingest(
            b1.data(), b1.size(), 6300, frame)));
}

void test_sequence_resync_after_silence_accepts_any_valid_sequence() {
    DdpAssembler assembler;
    RgbFrame frame;

    auto a0 = makePacket(2, 0, 1440, false);
    auto a1 = makePacket(2, 1440, 900, true);

    assembler.ingest(a0.data(), a0.size(), 7000, frame);
    assembler.ingest(a1.data(), a1.size(), 7100, frame);

    // Sequence 11 is outside the normal half-ring "newer" window from 2,
    // but after a long stream silence it must become a new baseline.
    auto b0 = makePacket(11, 0, 1440, false);
    auto b1 = makePacket(11, 1440, 900, true);

    const std::uint64_t resumedUs =
        7100 + DdpAssembler::kSequenceResyncSilenceUs + 1;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DdpIngestResult::Partial),
        static_cast<int>(assembler.ingest(
            b0.data(), b0.size(), resumedUs, frame)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DdpIngestResult::Complete),
        static_cast<int>(assembler.ingest(
            b1.data(), b1.size(), resumedUs + 100, frame)));

    TEST_ASSERT_EQUAL_UINT32(1, assembler.stats().sequenceResyncs);
}


void test_reset_stream_clears_completed_sequence_epoch() {
    DdpAssembler assembler;
    RgbFrame frame;

    auto a0 =
        makePacket(
            10,
            0,
            1440,
            false);

    auto a1 =
        makePacket(
            10,
            1440,
            900,
            true);

    assembler.ingest(
        a0.data(),
        a0.size(),
        1000,
        frame);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            DdpIngestResult::Complete),
        static_cast<int>(
            assembler.ingest(
                a1.data(),
                a1.size(),
                1100,
                frame)));

    // Sequence 3 would normally be stale immediately after completed 10.
    auto b0 =
        makePacket(
            3,
            0,
            1440,
            false);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            DdpIngestResult::Stale),
        static_cast<int>(
            assembler.ingest(
                b0.data(),
                b0.size(),
                1200,
                frame)));

    assembler.resetStream();

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            DdpIngestResult::Partial),
        static_cast<int>(
            assembler.ingest(
                b0.data(),
                b0.size(),
                1300,
                frame)));
}

void test_timeout_discards_incomplete_frame() {
    DdpAssembler assembler(1000);
    RgbFrame frame;

    auto first = makePacket(7, 0, 1440, false);

    assembler.ingest(first.data(), first.size(), 10000, frame);

    TEST_ASSERT_FALSE(assembler.expire(11000));
    TEST_ASSERT_TRUE(assembler.expire(11001));
    TEST_ASSERT_FALSE(assembler.active());
    TEST_ASSERT_EQUAL_UINT32(1, assembler.stats().timedOut);
}

void test_wrong_type_destination_and_bounds_are_rejected() {
    DdpAssembler assembler;
    RgbFrame frame;

    auto wrongType = makePacket(
        8, 0, 100, true, 0x1B, ambilight::kDdpDestination);
    auto wrongDestination = makePacket(
        8, 0, 100, true, ambilight::kDdpRgbDataType, 2);
    auto outOfBounds = makePacket(8, 2300, 100, true);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DdpIngestResult::Rejected),
        static_cast<int>(assembler.ingest(
            wrongType.data(), wrongType.size(), 12000, frame)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DdpIngestResult::Rejected),
        static_cast<int>(assembler.ingest(
            wrongDestination.data(), wrongDestination.size(), 12100, frame)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DdpIngestResult::Rejected),
        static_cast<int>(assembler.ingest(
            outOfBounds.data(), outOfBounds.size(), 12200, frame)));
}

void test_random_datagrams_preserve_assembler_invariants() {
    DdpAssembler assembler;
    RgbFrame frame;

    std::uint32_t state = 0xC6A5D31Fu;
    std::uint64_t nowUs = 20000;

    auto nextRandom = [&state]() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    };

    for (std::size_t iteration = 0; iteration < 5000; ++iteration) {
        const std::size_t length =
            static_cast<std::size_t>(nextRandom() % 1600U);

        std::vector<std::uint8_t> packet(length);
        for (std::size_t index = 0; index < packet.size(); ++index) {
            packet[index] =
                static_cast<std::uint8_t>(nextRandom() & 0xFFU);
        }

        nowUs += static_cast<std::uint64_t>(
            (nextRandom() % 2000U) + 1U);

        assembler.ingest(
            packet.empty() ? nullptr : packet.data(),
            packet.size(),
            nowUs,
            frame);

        TEST_ASSERT_TRUE(
            assembler.coveredBytes() <= assembler.frameBytes());

        if (assembler.active()) {
            TEST_ASSERT_TRUE(
                assembler.activeSequence() >= 1 &&
                assembler.activeSequence() <= 15);
        }
    }
}

void test_runtime_frame_size_reconfigures_completion_and_pixel_count() {
    DdpAssembler assembler;
    RgbFrame frame;

    constexpr std::size_t kPixels = 300;
    constexpr std::size_t kBytes = kPixels * 3;

    TEST_ASSERT_TRUE(
        assembler.setFrameBytes(
            kBytes));

    TEST_ASSERT_EQUAL_UINT32(
        kBytes,
        assembler.frameBytes());

    auto first =
        makePacket(
            12,
            0,
            600,
            false);

    auto second =
        makePacket(
            12,
            600,
            300,
            true);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            DdpIngestResult::Partial),
        static_cast<int>(
            assembler.ingest(
                first.data(),
                first.size(),
                1000,
                frame)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            DdpIngestResult::Complete),
        static_cast<int>(
            assembler.ingest(
                second.data(),
                second.size(),
                1100,
                frame)));

    TEST_ASSERT_EQUAL_UINT16(
        kPixels,
        frame.pixelCount);
}

void test_conflicting_overlap_rejects_active_frame() {
    DdpAssembler assembler;
    RgbFrame frame;

    auto first = makePacket(9, 0, 100, false);
    auto conflicting = makePacket(9, 50, 100, true);

    // Change one overlapping byte so that packet order could otherwise change
    // the final image.
    conflicting[ambilight::kDdpHeaderSize] ^= 0xFF;

    assembler.ingest(first.data(), first.size(), 13000, frame);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DdpIngestResult::Rejected),
        static_cast<int>(assembler.ingest(
            conflicting.data(), conflicting.size(), 13100, frame)));

    TEST_ASSERT_FALSE(assembler.active());
    TEST_ASSERT_EQUAL_UINT32(
        1,
        assembler.stats().conflictingDatagrams);
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_hyperhdr_two_packet_frame_completes);
    RUN_TEST(test_push_packet_can_arrive_first);
    RUN_TEST(test_duplicate_packet_does_not_fake_completion);
    RUN_TEST(test_new_sequence_supersedes_incomplete_frame);
    RUN_TEST(test_stale_sequence_is_rejected_after_completion);
    RUN_TEST(test_sequence_wrap_15_to_1_is_newer);
    RUN_TEST(test_sequence_resync_after_silence_accepts_any_valid_sequence);
    RUN_TEST(test_reset_stream_clears_completed_sequence_epoch);
    RUN_TEST(test_timeout_discards_incomplete_frame);
    RUN_TEST(test_wrong_type_destination_and_bounds_are_rejected);
    RUN_TEST(test_runtime_frame_size_reconfigures_completion_and_pixel_count);
    RUN_TEST(test_conflicting_overlap_rejects_active_frame);
    RUN_TEST(test_random_datagrams_preserve_assembler_invariants);

    return UNITY_END();
}
