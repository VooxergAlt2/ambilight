#include "tof/TofProcessor.h"

#include <algorithm>
#include <cstdlib>

namespace ambilight {

TofProcessor::TofProcessor(TofProcessorConfig config)
    : config_(config),
      planeEstimator_(config.plane) {}

void TofProcessor::reset() {
    leftFilter_ = {};
    centerFilter_ = {};
    rightFilter_ = {};
    latest_ = {};
}

void TofProcessor::setTransform(
    const TofGridTransform& transform) {

    config_.transform =
        transform;

    reset();
}

bool TofProcessor::statusUsable(std::uint8_t status) {
    return status == 5 || status == 6 || status == 9;
}

bool TofProcessor::sampleUsable(
    const TofRawFrame& raw,
    std::size_t normalizedRow,
    std::size_t normalizedCol,
    std::uint16_t& distanceMm) const {

    const std::size_t rawIndex = rawIndexForNormalized(
        normalizedRow,
        normalizedCol,
        config_.transform);

    const std::int16_t rawDistance = raw.distanceMm[rawIndex];
    const std::uint8_t status = raw.targetStatus[rawIndex];

    if (!statusUsable(status) || rawDistance <= 0) {
        return false;
    }

    const auto unsignedDistance =
        static_cast<std::uint16_t>(rawDistance);

    if (unsignedDistance < config_.minDistanceMm ||
        unsignedDistance > config_.maxDistanceMm) {
        return false;
    }

    distanceMm = unsignedDistance;
    return true;
}

std::uint16_t TofProcessor::median(
    std::array<std::uint16_t, 24>& values,
    std::size_t count) {

    if (count == 0) {
        return 0;
    }

    std::sort(values.begin(), values.begin() + count);

    if ((count & 1U) != 0U) {
        return values[count / 2];
    }

    const std::uint32_t a = values[count / 2 - 1];
    const std::uint32_t b = values[count / 2];

    return static_cast<std::uint16_t>((a + b) / 2U);
}

std::uint16_t TofProcessor::applyTemporalFilter(
    std::uint16_t measurementMm,
    std::uint64_t timestampUs,
    FilterState& state) {

    if (!state.initialized ||
        timestampUs <= state.timestampUs ||
        config_.filterTimeConstantMs == 0) {

        state.initialized = true;
        state.filteredMm = measurementMm;
        state.timestampUs = timestampUs;
        return measurementMm;
    }

    const std::int32_t delta =
        static_cast<std::int32_t>(measurementMm) -
        static_cast<std::int32_t>(state.filteredMm);

    if (std::abs(delta) <= config_.deadbandMm) {
        state.timestampUs = timestampUs;
        return state.filteredMm;
    }

    const std::uint64_t dtUs = timestampUs - state.timestampUs;
    const std::uint64_t tauUs =
        static_cast<std::uint64_t>(
            config_.filterTimeConstantMs) * 1000ULL;

    const std::uint64_t denominator = tauUs + dtUs;
    const std::uint32_t alphaQ16 =
        denominator == 0
            ? 65535U
            : static_cast<std::uint32_t>(
                  std::min<std::uint64_t>(
                      65535ULL,
                      (dtUs << 16) / denominator));

    const std::int64_t scaledDelta =
        static_cast<std::int64_t>(delta) *
        static_cast<std::int64_t>(alphaQ16);

    const std::int32_t filteredDelta =
        static_cast<std::int32_t>(
            scaledDelta / 65536LL);

    std::int32_t next =
        static_cast<std::int32_t>(state.filteredMm) +
        filteredDelta;

    if (next < 0) {
        next = 0;
    }

    if (next > 65535) {
        next = 65535;
    }

    state.filteredMm = static_cast<std::uint16_t>(next);
    state.timestampUs = timestampUs;

    return state.filteredMm;
}

TofBandEstimate TofProcessor::estimateBand(
    const TofRawFrame& raw,
    std::size_t firstColumn,
    std::size_t lastColumnInclusive,
    std::uint8_t minimumAccepted,
    FilterState& filterState) {

    TofBandEstimate result;

    std::array<std::uint16_t, 24> values{};
    std::size_t count = 0;

    for (std::size_t row = 0; row < kTofGridHeight; ++row) {
        for (std::size_t col = firstColumn;
             col <= lastColumnInclusive;
             ++col) {

            std::uint16_t distance = 0;
            if (!sampleUsable(raw, row, col, distance)) {
                continue;
            }

            values[count++] = distance;
        }
    }

    result.candidates = static_cast<std::uint8_t>(count);

    if (count == 0) {
        return result;
    }

    std::array<std::uint16_t, 24> sorted = values;
    result.rawMedianMm = median(sorted, count);

    std::array<std::uint16_t, 24> deviations{};
    for (std::size_t index = 0; index < count; ++index) {
        const std::int32_t deviation =
            static_cast<std::int32_t>(values[index]) -
            static_cast<std::int32_t>(result.rawMedianMm);

        deviations[index] = static_cast<std::uint16_t>(
            std::abs(deviation));
    }

    result.madMm = median(deviations, count);

    const std::uint32_t adaptiveWindow =
        static_cast<std::uint32_t>(result.madMm) *
        config_.madMultiplier;

    const std::uint32_t outlierWindow =
        std::max<std::uint32_t>(
            config_.minOutlierWindowMm,
            adaptiveWindow);

    std::array<std::uint16_t, 24> accepted{};
    std::size_t acceptedCount = 0;

    for (std::size_t index = 0; index < count; ++index) {
        const std::int32_t deviation =
            static_cast<std::int32_t>(values[index]) -
            static_cast<std::int32_t>(result.rawMedianMm);

        if (static_cast<std::uint32_t>(
                std::abs(deviation)) <= outlierWindow) {
            accepted[acceptedCount++] = values[index];
        }
    }

    result.accepted =
        static_cast<std::uint8_t>(acceptedCount);

    if (acceptedCount < minimumAccepted) {
        return result;
    }

    result.robustMedianMm =
        median(accepted, acceptedCount);

    result.filteredMm = applyTemporalFilter(
        result.robustMedianMm,
        raw.timestampUs,
        filterState);

    result.valid = true;
    return result;
}

TofGeometrySnapshot TofProcessor::process(
    const TofRawFrame& raw) {

    TofGeometrySnapshot next;

    next.generation = latest_.generation + 1;
    next.timestampUs = raw.timestampUs;

    next.left = estimateBand(
        raw,
        0,
        2,
        config_.minSideZones,
        leftFilter_);

    next.center = estimateBand(
        raw,
        3,
        4,
        config_.minCenterZones,
        centerFilter_);

    next.right = estimateBand(
        raw,
        5,
        7,
        config_.minSideZones,
        rightFilter_);

    next.plane =
        planeEstimator_.estimate(
            raw,
            config_.transform);

    next.acceptedZones = static_cast<std::uint8_t>(
        next.left.accepted +
        next.center.accepted +
        next.right.accepted);

    next.valid =
        next.left.valid &&
        next.center.valid &&
        next.right.valid;

    if (next.left.valid && next.right.valid) {
        const std::int32_t delta =
            static_cast<std::int32_t>(
                next.right.filteredMm) -
            static_cast<std::int32_t>(
                next.left.filteredMm);

        next.rightMinusLeftMm =
            static_cast<std::int16_t>(
                std::max<std::int32_t>(
                    -32768,
                    std::min<std::int32_t>(
                        32767,
                        delta)));
    }

    latest_ = next;
    return latest_;
}

} // namespace ambilight
