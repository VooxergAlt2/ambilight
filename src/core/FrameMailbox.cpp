#include "core/FrameMailbox.h"

namespace ambilight {

FrameMailbox::~FrameMailbox() {
    if (mutex_ != nullptr) {
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
    }
}

bool FrameMailbox::begin() {
    if (mutex_ != nullptr) {
        return true;
    }

    mutex_ = xSemaphoreCreateMutex();
    return mutex_ != nullptr;
}

bool FrameMailbox::publish(const RgbFrame& frame) {
    if (mutex_ == nullptr) {
        return false;
    }

    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    published_ = frame;
    published_.generation = ++generation_;

    xSemaphoreGive(mutex_);
    return true;
}

bool FrameMailbox::copyLatest(
    RgbFrame& destination,
    std::uint32_t lastGeneration) {

    if (mutex_ == nullptr) {
        return false;
    }

    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    const bool hasNewFrame = generation_ != 0 && generation_ != lastGeneration;
    if (hasNewFrame) {
        destination = published_;
    }

    xSemaphoreGive(mutex_);
    return hasNewFrame;
}

} // namespace ambilight
