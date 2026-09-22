#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <LiteLED.h>
#include <esp_err.h>

#include "config/BoardConfig.h"
#include "core/PerformanceMetric.h"
#include "core/RgbFrame.h"
#include "led/LedFrameWriteView.h"
#include "led/LedPhysicalPixelMask.h"

namespace ambilight {

// Thin owner of the ESP32-C6 PARLIO hardware.
//
// This class knows only physical lanes. Logical LED geometry belongs to
// LedRenderer and its prevalidated render plan.
class LedEngine {
public:
    LedEngine();

    esp_err_t begin(
        std::size_t physicalLaneLength =
            config::kDefaultPhysicalLaneLength);

    esp_err_t reconfigurePhysicalLaneLength(
        std::size_t physicalLaneLength);

    std::size_t physicalLaneLength() const {
        return physicalLaneLength_;
    }

    // Pipelined runtime submission. Encodes into the free DMA buffer while the
    // previous frame may still be on the wire, waits only for that older frame
    // if necessary, then submits the new frame and returns immediately.
    esp_err_t show();

    // Explicitly wait for the current in-flight frame. Used by startup,
    // diagnostics or controlled shutdown paths that require physical completion.
    esp_err_t waitForIdle();

    void setBrightness(
        std::uint8_t brightness);

    bool setPhysicalPixelMask(
        const LedPhysicalPixelMask& mask);

    const LedPhysicalPixelMask& physicalPixelMask() const {
        return physicalPixelMask_;
    }

    std::uint8_t brightness() const {
        return brightness_;
    }

    void clear();

    const LedFrameWriteView& frameWriteView() const {
        return frameWriteView_;
    }

    bool fillPhysicalRange(
        std::uint8_t lane,
        std::uint16_t start,
        std::uint16_t count,
        const Rgb8& color);

    const PerformanceMetric& encodeMetric() const {
        return encodeMetric_;
    }

    const PerformanceMetric& submitMetric() const {
        return submitMetric_;
    }

    const PerformanceMetric& waitMetric() const {
        return waitMetric_;
    }

    const PerformanceMetric& flushWaitMetric() const {
        return flushWaitMetric_;
    }

    const PerformanceMetric& showMetric() const {
        return showMetric_;
    }

    std::uint32_t overlappedShows() const {
        return overlappedShows_;
    }

    std::uint32_t coldShows() const {
        return coldShows_;
    }

    std::uint32_t submittedFrames() const {
        return submittedFrames_;
    }

    std::uint32_t completedFrames() const {
        return completedFrames_;
    }

    std::size_t dmaBufferBytes() const {
        return group_
            ? group_->dmaBufferBytes()
            : 0U;
    }

    std::uint8_t dmaBufferCount() const {
        return LiteLEDpioGroup::dmaBufferCount();
    }

    // Compatibility accessors retained while Stage 40 replaces the old STAT
    // surface with percentile-based performance diagnostics.
    std::uint32_t lastShowTimeUs() const {
        return static_cast<std::uint32_t>(
            showMetric_.lastUs());
    }

    std::uint32_t maxShowTimeUs() const {
        return static_cast<std::uint32_t>(
            showMetric_.maxUs());
    }

private:
    esp_err_t initializeGroup(
        std::size_t physicalLaneLength);

    void releaseGroup();

    std::optional<LiteLEDpioGroup> group_{};
    std::array<LiteLEDpioLane*, config::kParlioLaneCount> lanes_{};

    std::size_t physicalLaneLength_ = 0;

    std::uint8_t brightness_ =
        config::kDefaultOutputBrightness;

    bool begun_ = false;
    LedFrameWriteView frameWriteView_{};
    LedPhysicalPixelMask physicalPixelMask_{};

    PerformanceMetric encodeMetric_{};
    PerformanceMetric submitMetric_{};
    PerformanceMetric waitMetric_{};
    PerformanceMetric flushWaitMetric_{};
    PerformanceMetric showMetric_{};

    std::uint32_t overlappedShows_ = 0;
    std::uint32_t coldShows_ = 0;
    std::uint32_t submittedFrames_ = 0;
    std::uint32_t completedFrames_ = 0;
};

} // namespace ambilight
