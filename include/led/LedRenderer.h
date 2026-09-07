#pragma once

#include <cstdint>

#include <esp_err.h>

#include "core/PerformanceMetric.h"
#include "core/RgbFrame.h"
#include "led/LedEngine.h"
#include "led/LedMappingProfile.h"
#include "led/LedPixelMaskProfile.h"
#include "led/LedRenderPlan.h"
#include "render/RenderGainContext.h"

namespace ambilight {

class LedRenderer {
public:
    explicit LedRenderer(LedEngine& engine)
        : engine_(engine) {}

    bool setMappingProfile(
        const LedMappingProfile& profile);

    const LedMappingProfile& mappingProfile() const {
        return mappingProfile_;
    }

    const LedRenderPlan& renderPlan() const {
        return renderPlan_;
    }

    bool setPixelMaskProfile(
        const LedPixelMaskProfile& profile);

    const LedPixelMaskProfile& pixelMaskProfile() const {
        return pixelMaskProfile_;
    }

    // Uncorrected physical output. Used by DISABLED, SHADOW and commissioning.
    esp_err_t render(
        const RgbFrame& frame);

    // ACTIVE-only physical output. Gain is applied directly without running
    // shadow diagnostics or candidate-delta bookkeeping.
    esp_err_t renderActive(
        const RgbFrame& frame,
        const RenderGainContext& gainContext);

    std::uint32_t renderedFrames() const {
        return renderedFrames_;
    }

    std::uint32_t mappingErrors() const {
        return mappingErrors_;
    }

    const PerformanceMetric& prepareMetric() const {
        return prepareMetric_;
    }

    const PerformanceMetric& postMetric() const {
        return postMetric_;
    }

    const PerformanceMetric& renderMetric() const {
        return renderMetric_;
    }

private:
    bool validateFrame(
        const RgbFrame& frame) const;

    esp_err_t showPrepared(
        std::uint64_t renderStartedUs,
        std::uint64_t prepareStartedUs);

    LedEngine& engine_;

    std::uint32_t renderedFrames_ = 0;
    std::uint32_t mappingErrors_ = 0;

    LedMappingProfile mappingProfile_{};
    LedPixelMaskProfile pixelMaskProfile_{};
    LedRenderPlan renderPlan_{};

    PerformanceMetric prepareMetric_{};
    PerformanceMetric postMetric_{};
    PerformanceMetric renderMetric_{};
};

} // namespace ambilight
