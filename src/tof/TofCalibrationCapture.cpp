#include "tof/TofCalibrationCapture.h"

#include <algorithm>

namespace ambilight {

void TofCalibrationCapture::start(
    std::uint64_t nowUs,
    std::uint64_t durationUs) {

    active_ = true;

    startedUs_ = nowUs;
    durationUs_ =
        durationUs == 0
            ? kDefaultDurationUs
            : durationUs;

    lastGeometryGeneration_ = 0;

    totalFrames_ = 0;
    validFrames_ = 0;
    duplicateFrames_ = 0;
    overflowFrames_ = 0;
    storedSamples_ = 0;

    left_ = {};
    center_ = {};
    right_ = {};
    deltas_.fill(0);
}

void TofCalibrationCapture::cancel() {
    active_ = false;
}

bool TofCalibrationCapture::ingest(
    const TofGeometrySnapshot& geometry) {

    if (!active_) {
        return false;
    }

    if (geometry.generation == 0) {
        return false;
    }

    if (geometry.generation == lastGeometryGeneration_) {
        ++duplicateFrames_;
        return false;
    }

    lastGeometryGeneration_ = geometry.generation;
    ++totalFrames_;

    if (!geometry.valid ||
        !geometry.left.valid ||
        !geometry.center.valid ||
        !geometry.right.valid) {
        return false;
    }

    ++validFrames_;

    if (storedSamples_ >= kMaxSamples) {
        ++overflowFrames_;
        return false;
    }

    const std::size_t index = storedSamples_;

    left_.distanceMm[index] =
        geometry.left.robustMedianMm;
    left_.madMm[index] =
        geometry.left.madMm;
    left_.accepted[index] =
        geometry.left.accepted;

    center_.distanceMm[index] =
        geometry.center.robustMedianMm;
    center_.madMm[index] =
        geometry.center.madMm;
    center_.accepted[index] =
        geometry.center.accepted;

    right_.distanceMm[index] =
        geometry.right.robustMedianMm;
    right_.madMm[index] =
        geometry.right.madMm;
    right_.accepted[index] =
        geometry.right.accepted;

    const std::int32_t delta =
        static_cast<std::int32_t>(
            geometry.right.robustMedianMm) -
        static_cast<std::int32_t>(
            geometry.left.robustMedianMm);

    deltas_[index] =
        static_cast<std::int16_t>(
            std::max<std::int32_t>(
                -32768,
                std::min<std::int32_t>(
                    32767,
                    delta)));

    ++storedSamples_;
    return true;
}

bool TofCalibrationCapture::expired(
    std::uint64_t nowUs) const {

    if (!active_) {
        return false;
    }

    if (nowUs < startedUs_) {
        return false;
    }

    return nowUs - startedUs_ >= durationUs_;
}

std::uint16_t TofCalibrationCapture::percentile(
    std::array<std::uint16_t, kMaxSamples> values,
    std::size_t count,
    std::uint8_t percentileValue) {

    if (count == 0) {
        return 0;
    }

    if (percentileValue > 100) {
        percentileValue = 100;
    }

    std::sort(values.begin(), values.begin() + count);

    const std::size_t index =
        count == 1
            ? 0
            : static_cast<std::size_t>(
                  (static_cast<std::uint64_t>(count - 1) *
                   percentileValue +
                   50U) /
                  100U);

    return values[index];
}

CalibrationBandSummary TofCalibrationCapture::summarizeBand(
    const BandSamples& samples,
    std::size_t count) {

    CalibrationBandSummary summary;

    if (count == 0) {
        return summary;
    }

    summary.valid = true;

    summary.p10Mm =
        percentile(samples.distanceMm, count, 10);
    summary.medianMm =
        percentile(samples.distanceMm, count, 50);
    summary.p90Mm =
        percentile(samples.distanceMm, count, 90);

    summary.medianMadMm =
        percentile(samples.madMm, count, 50);

    summary.minAccepted = samples.accepted[0];
    summary.maxAccepted = samples.accepted[0];

    for (std::size_t index = 1; index < count; ++index) {
        summary.minAccepted =
            std::min(
                summary.minAccepted,
                samples.accepted[index]);

        summary.maxAccepted =
            std::max(
                summary.maxAccepted,
                samples.accepted[index]);
    }

    return summary;
}

std::int16_t TofCalibrationCapture::summarizeDelta(
    std::array<std::int16_t, kMaxSamples> values,
    std::size_t count) {

    if (count == 0) {
        return 0;
    }

    std::sort(values.begin(), values.begin() + count);

    if ((count & 1U) != 0U) {
        return values[count / 2];
    }

    const std::int32_t a = values[count / 2 - 1];
    const std::int32_t b = values[count / 2];

    return static_cast<std::int16_t>(
        (a + b) / 2);
}

CalibrationCaptureSummary TofCalibrationCapture::finish() {
    CalibrationCaptureSummary summary;

    summary.totalFrames = totalFrames_;
    summary.validFrames = validFrames_;
    summary.duplicateFrames = duplicateFrames_;
    summary.overflowFrames = overflowFrames_;

    summary.left =
        summarizeBand(left_, storedSamples_);

    summary.center =
        summarizeBand(center_, storedSamples_);

    summary.right =
        summarizeBand(right_, storedSamples_);

    summary.medianRightMinusLeftMm =
        summarizeDelta(
            deltas_,
            storedSamples_);

    active_ = false;
    return summary;
}

} // namespace ambilight
