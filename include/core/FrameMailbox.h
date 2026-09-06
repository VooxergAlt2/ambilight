#pragma once

#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "core/RgbFrame.h"

namespace ambilight {

// Small task-level mailbox for complete RGB frames.
//
// Correctness is deliberately preferred over premature zero-copy tricks.
// The mailbox uses the fixed 920-pixel capacity but every frame carries its
// active runtime pixelCount. Copy cost stays small relative to the C6 memory
// bandwidth and avoids lifetime/zero-copy hazards during topology changes.
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
