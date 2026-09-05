#include "transport/AwaParser.h"

namespace ambilight {

void AwaParser::resetFrameState() {
    version2_ = false;

    encodedCount_ = 0;
    headerCrc_ = 0;

    payloadIndex_ = 0;
    calibrationRemaining_ = 0;

    fletcher1_ = 0;
    fletcher2_ = 0;
    fletcherExt_ = 0;
    fletcherPosition_ = 0;
}

void AwaParser::reset() {
    state_ = State::HeaderA;
    resetFrameState();
}

void AwaParser::resyncFromByte(std::uint8_t input) {
    resetFrameState();

    // Preserve an overlapping start marker. For example, "AAwa..." should
    // recover using the second 'A' rather than discarding it.
    state_ =
        input == static_cast<std::uint8_t>('A')
            ? State::HeaderSecond
            : State::HeaderA;
}

void AwaParser::addFletcher(std::uint8_t input) {
    fletcher1_ =
        (fletcher1_ + static_cast<std::uint16_t>(input)) % 255U;

    fletcher2_ =
        (fletcher2_ + fletcher1_) % 255U;

    fletcherExt_ =
        (fletcherExt_ +
         static_cast<std::uint16_t>(
             input ^ fletcherPosition_)) %
        255U;

    ++fletcherPosition_;
}

std::uint8_t AwaParser::expectedFletcherExt() const {
    return static_cast<std::uint8_t>(
        fletcherExt_ != 0x41U
            ? fletcherExt_
            : 0xAAU);
}

AwaEvent AwaParser::consume(
    std::uint8_t input,
    RgbFrame& completedFrame) {

    ++stats_.bytes;

    switch (state_) {
    case State::HeaderA:
        if (input == static_cast<std::uint8_t>('A')) {
            state_ = State::HeaderSecond;
        }
        return AwaEvent::None;

    case State::HeaderSecond:
        if (input == static_cast<std::uint8_t>('w')) {
            state_ = State::HeaderThird;
            return AwaEvent::None;
        }

        if (input == static_cast<std::uint8_t>('W')) {
            ++stats_.unsupportedVersion3;
        } else {
            ++stats_.headerErrors;
        }

        resyncFromByte(input);
        return AwaEvent::None;

    case State::HeaderThird:
        if (input == static_cast<std::uint8_t>('a')) {
            version2_ = false;
            state_ = State::HeaderHi;
            return AwaEvent::None;
        }

        if (input == static_cast<std::uint8_t>('A')) {
            version2_ = true;
            state_ = State::HeaderHi;
            return AwaEvent::None;
        }

        ++stats_.headerErrors;
        resyncFromByte(input);
        return AwaEvent::None;

    case State::HeaderHi:
        encodedCount_ =
            static_cast<std::uint16_t>(input) << 8;
        headerCrc_ = input;

        fletcher1_ = 0;
        fletcher2_ = 0;
        fletcherExt_ = 0;
        fletcherPosition_ = 0;
        payloadIndex_ = 0;

        state_ = State::HeaderLo;
        return AwaEvent::None;

    case State::HeaderLo:
        encodedCount_ = static_cast<std::uint16_t>(
            encodedCount_ + input);
        headerCrc_ =
            static_cast<std::uint8_t>(
                headerCrc_ ^ input ^ 0x55U);

        state_ = State::HeaderCrc;
        return AwaEvent::None;

    case State::HeaderCrc: {
        if (headerCrc_ != input) {
            if (encodedCount_ == 0x2AA2U && input == 0x15U) {
                ++stats_.handshakeRequests;
                reset();
                return AwaEvent::HandshakeRequest;
            }

            if (encodedCount_ == 0x2AA2U && input == 0x35U) {
                ++stats_.sleepRequests;
                reset();
                return AwaEvent::SleepRequest;
            }

            ++stats_.headerErrors;
            resyncFromByte(input);
            return AwaEvent::None;
        }

        const std::uint32_t ledCount =
            static_cast<std::uint32_t>(encodedCount_) + 1U;

        if (ledCount != config::kLogicalLedCount) {
            ++stats_.wrongLedCount;
            resyncFromByte(input);
            return AwaEvent::None;
        }

        ++stats_.framesStarted;
        if (version2_) {
            ++stats_.version2Frames;
        }

        state_ = State::Payload;
        return AwaEvent::None;
    }

    case State::Payload: {
        auto* bytes = reinterpret_cast<std::uint8_t*>(
            staging_.pixels.data());

        bytes[payloadIndex_++] = input;
        addFletcher(input);

        if (payloadIndex_ == kPayloadBytes) {
            if (version2_) {
                calibrationRemaining_ = 4;
                state_ = State::Calibration;
            } else {
                state_ = State::Fletcher1;
            }
        }

        return AwaEvent::None;
    }

    case State::Calibration:
        addFletcher(input);

        if (--calibrationRemaining_ == 0) {
            state_ = State::Fletcher1;
        }

        return AwaEvent::None;

    case State::Fletcher1:
        if (input != static_cast<std::uint8_t>(fletcher1_)) {
            ++stats_.checksumErrors;
            resyncFromByte(input);
            return AwaEvent::None;
        }

        state_ = State::Fletcher2;
        return AwaEvent::None;

    case State::Fletcher2:
        if (input != static_cast<std::uint8_t>(fletcher2_)) {
            ++stats_.checksumErrors;
            resyncFromByte(input);
            return AwaEvent::None;
        }

        state_ = State::FletcherExt;
        return AwaEvent::None;

    case State::FletcherExt:
        if (input != expectedFletcherExt()) {
            ++stats_.checksumErrors;
            resyncFromByte(input);
            return AwaEvent::None;
        }

        completedFrame = staging_;
        completedFrame.generation = 0;
        completedFrame.receivedUs = 0;

        ++stats_.goodFrames;

        reset();
        return AwaEvent::FrameComplete;
    }

    reset();
    return AwaEvent::None;
}

} // namespace ambilight
