#pragma once

#include <cstdint>

namespace liteled_parlio {

template<std::uint8_t BufferCount>
class PipelineState {
public:
    static_assert(
        BufferCount >= 2,
        "PARLIO pipeline requires at least two buffers");

    static constexpr std::uint8_t kInvalid =
        0xFF;

    void reset() {
        encodedReady_ = false;
        inFlight_ = false;

        nextEncode_ = 0;
        ready_ = kInvalid;
        inFlightBuffer_ = kInvalid;
    }

    bool canEncode() const {
        return
            !encodedReady_ &&
            nextEncode_ < BufferCount &&
            (
                !inFlight_ ||
                nextEncode_ !=
                    inFlightBuffer_
            );
    }

    std::uint8_t nextEncodeBuffer() const {
        return nextEncode_;
    }

    void markEncoded() {
        ready_ =
            nextEncode_;

        encodedReady_ =
            true;
    }

    bool encodedReady() const {
        return encodedReady_;
    }

    std::uint8_t readyBuffer() const {
        return ready_;
    }

    bool canTransmit() const {
        return
            encodedReady_ &&
            !inFlight_ &&
            ready_ < BufferCount;
    }

    void markTransmitted() {
        inFlight_ = true;
        inFlightBuffer_ =
            ready_;

        encodedReady_ = false;
        ready_ = kInvalid;

        nextEncode_ =
            static_cast<std::uint8_t>(
                (
                    inFlightBuffer_ +
                    1U
                ) %
                BufferCount);
    }

    bool inFlight() const {
        return inFlight_;
    }

    std::uint8_t inFlightBuffer() const {
        return inFlightBuffer_;
    }

    bool canWait() const {
        return inFlight_;
    }

    void markWaited() {
        inFlight_ = false;
        inFlightBuffer_ =
            kInvalid;
    }

private:
    bool encodedReady_ = false;
    bool inFlight_ = false;

    std::uint8_t nextEncode_ = 0;
    std::uint8_t ready_ = kInvalid;
    std::uint8_t inFlightBuffer_ =
        kInvalid;
};

} // namespace liteled_parlio
