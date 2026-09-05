#include "tof/TofCalibrationCapture.h"

#include <algorithm>
#include <cmath>

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
    planeSamplesCount_ = 0;
    spatialSamplesCount_ = 0;

    left_ = {};
    center_ = {};
    right_ = {};
    deltas_.fill(0);

    plane_ = {};
    spatial_ = {};
}

void TofCalibrationCapture::cancel() {
    active_ = false;
}

bool TofCalibrationCapture::ingest(
    const TofGeometrySnapshot& geometry) {

    return ingest(
        geometry,
        PerimeterGainSnapshot{});
}

bool TofCalibrationCapture::ingest(
    const TofGeometrySnapshot& geometry,
    const PerimeterGainSnapshot& spatial) {

    if (!active_ ||
        geometry.generation == 0) {
        return false;
    }

    if (geometry.generation ==
        lastGeometryGeneration_) {

        ++duplicateFrames_;
        return false;
    }

    lastGeometryGeneration_ =
        geometry.generation;

    ++totalFrames_;

    bool captured = false;
    bool overflowed = false;

    const bool bandsValid =
        geometry.valid &&
        geometry.left.valid &&
        geometry.center.valid &&
        geometry.right.valid;

    if (bandsValid) {
        ++validFrames_;

        if (storedSamples_ <
            kMaxSamples) {

            captured |=
                captureBands(
                    geometry);
        } else {
            overflowed = true;
        }
    }

    if (geometry.plane.valid) {
        if (planeSamplesCount_ <
            kMaxSamples) {

            captured |=
                capturePlane(
                    geometry.plane);
        } else {
            overflowed = true;
        }
    }

    const bool spatialValid =
        spatial.planeUsable &&
        spatial.projectionUsable &&
        !spatial.failOpen;

    if (spatialValid) {
        if (spatialSamplesCount_ <
            kMaxSamples) {

            captured |=
                captureSpatial(
                    spatial);
        } else {
            overflowed = true;
        }
    }

    if (overflowed) {
        ++overflowFrames_;
    }

    return captured;
}

bool TofCalibrationCapture::captureBands(
    const TofGeometrySnapshot& geometry) {

    if (storedSamples_ >=
        kMaxSamples) {
        return false;
    }

    const std::size_t index =
        storedSamples_;

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

std::uint16_t TofCalibrationCapture::clampUnsignedMm(
    float value) {

    if (!std::isfinite(value) ||
        value <= 0.0F) {
        return 0;
    }

    if (value >= 65535.0F) {
        return 65535;
    }

    return static_cast<std::uint16_t>(
        std::lround(value));
}

bool TofCalibrationCapture::capturePlane(
    const TofPlaneEstimate& plane) {

    if (!plane.valid ||
        planeSamplesCount_ >=
            kMaxSamples) {
        return false;
    }

    const std::size_t index =
        planeSamplesCount_;

    plane_.yawCentiDeg[index] =
        plane.yawCentiDeg;

    plane_.pitchCentiDeg[index] =
        plane.pitchCentiDeg;

    plane_.interceptMm[index] =
        clampUnsignedMm(
            plane.interceptMm);

    plane_.residualMadMm[index] =
        plane.residualMadMm;

    plane_.accepted[index] =
        plane.accepted;

    plane_.observedHalfSpanXmm[index] =
        plane.observedHalfSpanXmm;

    plane_.observedHalfSpanYmm[index] =
        plane.observedHalfSpanYmm;

    ++planeSamplesCount_;
    return true;
}

bool TofCalibrationCapture::captureSpatial(
    const PerimeterGainSnapshot& spatial) {

    if (!spatial.planeUsable ||
        !spatial.projectionUsable ||
        spatial.failOpen ||
        spatialSamplesCount_ >=
            kMaxSamples) {
        return false;
    }

    const std::size_t index =
        spatialSamplesCount_;

    spatial_.minDistanceMm[index] =
        spatial.minDistanceMm;

    spatial_.maxDistanceMm[index] =
        spatial.maxDistanceMm;

    for (std::size_t segmentIndex = 0;
         segmentIndex <
            spatial.segment.size();
         ++segmentIndex) {

        spatial_.startDistanceMm[
            segmentIndex][index] =
            spatial.segment[
                segmentIndex].
                startDistanceMm;

        spatial_.endDistanceMm[
            segmentIndex][index] =
            spatial.segment[
                segmentIndex].
                endDistanceMm;
    }

    ++spatialSamplesCount_;
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

    return nowUs - startedUs_ >=
        durationUs_;
}

std::uint16_t TofCalibrationCapture::percentile(
    std::array<
        std::uint16_t,
        kMaxSamples> values,
    std::size_t count,
    std::uint8_t percentileValue) {

    if (count == 0) {
        return 0;
    }

    percentileValue =
        std::min<std::uint8_t>(
            100,
            percentileValue);

    std::sort(
        values.begin(),
        values.begin() + count);

    const std::size_t index =
        count == 1
            ? 0
            : static_cast<std::size_t>(
                  (
                      static_cast<std::uint64_t>(
                          count - 1) *
                      percentileValue +
                      50U
                  ) /
                  100U);

    return values[index];
}

std::int16_t TofCalibrationCapture::percentileSigned(
    std::array<
        std::int16_t,
        kMaxSamples> values,
    std::size_t count,
    std::uint8_t percentileValue) {

    if (count == 0) {
        return 0;
    }

    percentileValue =
        std::min<std::uint8_t>(
            100,
            percentileValue);

    std::sort(
        values.begin(),
        values.begin() + count);

    const std::size_t index =
        count == 1
            ? 0
            : static_cast<std::size_t>(
                  (
                      static_cast<std::uint64_t>(
                          count - 1) *
                      percentileValue +
                      50U
                  ) /
                  100U);

    return values[index];
}

CalibrationUnsignedSummary
TofCalibrationCapture::summarizeUnsigned(
    const std::array<
        std::uint16_t,
        kMaxSamples>& values,
    std::size_t count) {

    CalibrationUnsignedSummary summary;

    if (count == 0) {
        return summary;
    }

    summary.valid = true;
    summary.p10 =
        percentile(
            values,
            count,
            10);
    summary.median =
        percentile(
            values,
            count,
            50);
    summary.p90 =
        percentile(
            values,
            count,
            90);

    return summary;
}

CalibrationSignedSummary
TofCalibrationCapture::summarizeSigned(
    const std::array<
        std::int16_t,
        kMaxSamples>& values,
    std::size_t count) {

    CalibrationSignedSummary summary;

    if (count == 0) {
        return summary;
    }

    summary.valid = true;
    summary.p10 =
        percentileSigned(
            values,
            count,
            10);
    summary.median =
        percentileSigned(
            values,
            count,
            50);
    summary.p90 =
        percentileSigned(
            values,
            count,
            90);

    return summary;
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
        percentile(
            samples.distanceMm,
            count,
            10);

    summary.medianMm =
        percentile(
            samples.distanceMm,
            count,
            50);

    summary.p90Mm =
        percentile(
            samples.distanceMm,
            count,
            90);

    summary.medianMadMm =
        percentile(
            samples.madMm,
            count,
            50);

    summary.minAccepted =
        samples.accepted[0];

    summary.maxAccepted =
        samples.accepted[0];

    for (std::size_t index = 1;
         index < count;
         ++index) {

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
    std::array<
        std::int16_t,
        kMaxSamples> values,
    std::size_t count) {

    if (count == 0) {
        return 0;
    }

    std::sort(
        values.begin(),
        values.begin() + count);

    if ((count & 1U) != 0U) {
        return values[
            count / 2];
    }

    const std::int32_t a =
        values[
            count / 2 - 1];

    const std::int32_t b =
        values[
            count / 2];

    return static_cast<std::int16_t>(
        (a + b) / 2);
}

CalibrationCaptureSummary TofCalibrationCapture::finish() {
    CalibrationCaptureSummary summary;

    summary.totalFrames =
        totalFrames_;

    summary.validFrames =
        validFrames_;

    summary.duplicateFrames =
        duplicateFrames_;

    summary.overflowFrames =
        overflowFrames_;

    summary.left =
        summarizeBand(
            left_,
            storedSamples_);

    summary.center =
        summarizeBand(
            center_,
            storedSamples_);

    summary.right =
        summarizeBand(
            right_,
            storedSamples_);

    summary.medianRightMinusLeftMm =
        summarizeDelta(
            deltas_,
            storedSamples_);

    if (planeSamplesCount_ > 0) {
        summary.plane.valid = true;
        summary.plane.validFrames =
            planeSamplesCount_;

        summary.plane.yawCentiDeg =
            summarizeSigned(
                plane_.yawCentiDeg,
                planeSamplesCount_);

        summary.plane.pitchCentiDeg =
            summarizeSigned(
                plane_.pitchCentiDeg,
                planeSamplesCount_);

        summary.plane.interceptMm =
            summarizeUnsigned(
                plane_.interceptMm,
                planeSamplesCount_);

        summary.plane.medianResidualMadMm =
            percentile(
                plane_.residualMadMm,
                planeSamplesCount_,
                50);

        summary.plane.minAccepted =
            plane_.accepted[0];

        summary.plane.maxAccepted =
            plane_.accepted[0];

        for (std::size_t index = 1;
             index < planeSamplesCount_;
             ++index) {

            summary.plane.minAccepted =
                std::min(
                    summary.plane.minAccepted,
                    plane_.accepted[index]);

            summary.plane.maxAccepted =
                std::max(
                    summary.plane.maxAccepted,
                    plane_.accepted[index]);
        }

        summary.plane.medianObservedHalfSpanXmm =
            percentile(
                plane_.observedHalfSpanXmm,
                planeSamplesCount_,
                50);

        summary.plane.medianObservedHalfSpanYmm =
            percentile(
                plane_.observedHalfSpanYmm,
                planeSamplesCount_,
                50);
    }

    if (spatialSamplesCount_ > 0) {
        summary.spatial.valid = true;
        summary.spatial.validFrames =
            spatialSamplesCount_;

        summary.spatial.minDistanceMm =
            summarizeUnsigned(
                spatial_.minDistanceMm,
                spatialSamplesCount_);

        summary.spatial.maxDistanceMm =
            summarizeUnsigned(
                spatial_.maxDistanceMm,
                spatialSamplesCount_);

        for (std::size_t segmentIndex = 0;
             segmentIndex <
                summary.spatial.segment.size();
             ++segmentIndex) {

            summary.spatial.segment[
                segmentIndex].startMm =
                summarizeUnsigned(
                    spatial_.startDistanceMm[
                        segmentIndex],
                    spatialSamplesCount_);

            summary.spatial.segment[
                segmentIndex].endMm =
                summarizeUnsigned(
                    spatial_.endDistanceMm[
                        segmentIndex],
                    spatialSamplesCount_);
        }
    }

    active_ = false;
    return summary;
}

} // namespace ambilight
