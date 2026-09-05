#pragma once

#include <cstdint>

#include <esp_err.h>

#include "core/RgbFrame.h"
#include "led/LedEngine.h"

namespace ambilight {

class LedRenderer {
public:
    explicit LedRenderer(LedEngine& engine)
        : engine_(engine) {}

    esp_err_t render(const RgbFrame& frame);

    std::uint32_t renderedFrames() const { return renderedFrames_; }
    std::uint32_t mappingErrors() const { return mappingErrors_; }

private:
    static crgb_t toCrgb(const Rgb8& color);

    LedEngine& engine_;
    std::uint32_t renderedFrames_ = 0;
    std::uint32_t mappingErrors_ = 0;
};

} // namespace ambilight
