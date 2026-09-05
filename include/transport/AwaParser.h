#pragma once

#include <cstddef>
#include <cstdint>

#include "core/RgbFrame.h"

namespace ambilight {

enum class AwaEvent : std::uint8_t {
    None = 0,
    FrameComplete,
    HandshakeRequest,
    SleepRequest
};

struct AwaParserStats {
    std::uint64_t bytes = 0;
    std::uint32_t framesStarted = 0;
    std::uint32_t goodFrames = 0;
    std::uint32_t headerErrors = 0;
    std::uint32_t wrongLedCount = 0;
    std::uint32_t checksumErrors = 0;
    std::uint32_t version2Frames = 0;
    std::uint32_t unsupportedVersion3 = 0;
    std::uint32_t handshakeRequests = 0;
    std::uint32_t sleepRequests = 0;
};

class AwaParser {
public:
    static constexpr std::size_t kPayloadBytes =
        config::kLogicalLedCount * sizeof(Rgb8);

    AwaEvent consume(std::uint8_t input, RgbFrame& completedFrame);

    void reset();

    const AwaParserStats& stats() const { return stats_; }

private:
    enum class State : std::uint8_t {
        HeaderA = 0,
        HeaderSecond,
        HeaderThird,
        HeaderHi,
        HeaderLo,
        HeaderCrc,
        Payload,
        Calibration,
        Fletcher1,
        Fletcher2,
        FletcherExt
    };

    void resetFrameState();
    void resyncFromByte(std::uint8_t input);
    void addFletcher(std::uint8_t input);
    std::uint8_t expectedFletcherExt() const;

    State state_ = State::HeaderA;

    bool version2_ = false;

    std::uint16_t encodedCount_ = 0;
    std::uint8_t headerCrc_ = 0;

    std::size_t payloadIndex_ = 0;
    std::uint8_t calibrationRemaining_ = 0;

    std::uint16_t fletcher1_ = 0;
    std::uint16_t fletcher2_ = 0;
    std::uint16_t fletcherExt_ = 0;
    std::uint8_t fletcherPosition_ = 0;

    RgbFrame staging_{};
    AwaParserStats stats_{};
};

static_assert(AwaParser::kPayloadBytes == 2340);

} // namespace ambilight
