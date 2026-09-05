#pragma once

#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "core/RgbFrame.h"

namespace ambilight {

// Small task-level mailbox for complete RGB frames.
//
// Correctness is deliberately preferred over premature zero-copy tricks.
// One 780-pixel RGB frame is 2340 bytes. At 60 FPS, one full-frame copy is
// about 140 KB/s; publish + consume is about 280 KB/s total.
//
// The mutex is a task mutex, not an interrupt-disabling critical section.
class FrameMailbox {
public:
    FrameMailbox() = default;
    ~FrameMailbox();

    FrameMailbox(const FrameMailbox&) = delete;
    FrameMailbox& operator=(const FrameMailbox&) = delete;

    bool begin();

    bool publish(const RgbFrame& frame);
    bool copyLatest(RgbFrame& destination, std::uint32_t lastGeneration);

private:
    SemaphoreHandle_t mutex_ = nullptr;
    RgbFrame published_{};
    std::uint32_t generation_ = 0;
};

} // namespace ambilight
