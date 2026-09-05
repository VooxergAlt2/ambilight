#include "led/LedRenderer.h"

#include "led/SegmentMapper.h"

namespace ambilight {

crgb_t LedRenderer::toCrgb(const Rgb8& color) {
    return
        (static_cast<crgb_t>(color.r) << 16) |
        (static_cast<crgb_t>(color.g) << 8) |
        static_cast<crgb_t>(color.b);
}

esp_err_t LedRenderer::render(const RgbFrame& frame) {
    for (std::uint16_t logical = 0;
         logical < config::kLogicalLedCount;
         ++logical) {

        const PhysicalPixel mapped = SegmentMapper::map(logical);
        if (!mapped.valid) {
            ++mappingErrors_;
            return ESP_ERR_INVALID_ARG;
        }

        if (!engine_.setPhysicalPixel(
                mapped.lane,
                mapped.index,
                toCrgb(frame.pixels[logical]))) {
            ++mappingErrors_;
            return ESP_FAIL;
        }
    }

    const esp_err_t result = engine_.show();
    if (result == ESP_OK) {
        ++renderedFrames_;
    }

    return result;
}

} // namespace ambilight
