#pragma once

#include <cstddef>
#include <cstdint>

namespace liteled_parlio {

constexpr std::uint8_t kDataWidth = 8;
constexpr std::uint8_t kMaxSamplesPerBit = 8;

enum class SampleMode : std::uint8_t {
    ConstantZero = 0,
    ConstantOne,
    Data,
    InvertedData
};

struct SamplePlan {
    std::uint8_t samplesPerBit = 0;
    SampleMode mode[kMaxSamplesPerBit]{};

    std::uint8_t dynamicCount = 0;
    std::uint8_t dynamicSample[
        kMaxSamplesPerBit]{};
    bool dynamicInverted[
        kMaxSamplesPerBit]{};

    constexpr bool valid() const {
        return
            samplesPerBit > 0 &&
            samplesPerBit <=
                kMaxSamplesPerBit;
    }
};

constexpr SamplePlan makeSamplePlan(
    std::uint8_t bit0Pattern,
    std::uint8_t bit1Pattern,
    std::uint8_t samplesPerBit) {

    SamplePlan plan;

    if (samplesPerBit == 0 ||
        samplesPerBit >
            kMaxSamplesPerBit) {

        return plan;
    }

    plan.samplesPerBit =
        samplesPerBit;

    for (std::uint8_t sample = 0;
         sample < samplesPerBit;
         ++sample) {

        const std::uint8_t shift =
            static_cast<std::uint8_t>(
                samplesPerBit -
                1U -
                sample);

        const bool zeroLevel =
            ((bit0Pattern >> shift) & 1U) != 0;

        const bool oneLevel =
            ((bit1Pattern >> shift) & 1U) != 0;

        if (!zeroLevel &&
            !oneLevel) {

            plan.mode[sample] =
                SampleMode::ConstantZero;
        } else if (
            zeroLevel &&
            oneLevel) {

            plan.mode[sample] =
                SampleMode::ConstantOne;
        } else if (
            !zeroLevel &&
            oneLevel) {

            plan.mode[sample] =
                SampleMode::Data;

            plan.dynamicSample[
                plan.dynamicCount] =
                sample;

            plan.dynamicInverted[
                plan.dynamicCount] =
                false;

            ++plan.dynamicCount;
        } else {
            plan.mode[sample] =
                SampleMode::InvertedData;

            plan.dynamicSample[
                plan.dynamicCount] =
                sample;

            plan.dynamicInverted[
                plan.dynamicCount] =
                true;

            ++plan.dynamicCount;
        }
    }

    return plan;
}

inline void initializeEncodedByte(
    std::uint8_t* output,
    const SamplePlan& plan,
    std::uint8_t activeLaneMask) {

    if (output == nullptr ||
        !plan.valid()) {

        return;
    }

    for (std::uint8_t bitSlot = 0;
         bitSlot < 8;
         ++bitSlot) {

        const std::size_t base =
            static_cast<std::size_t>(
                bitSlot) *
            plan.samplesPerBit;

        for (std::uint8_t sample = 0;
             sample <
                plan.samplesPerBit;
             ++sample) {

            switch (plan.mode[sample]) {
            case SampleMode::ConstantZero:
                output[base + sample] = 0;
                break;

            case SampleMode::ConstantOne:
                output[base + sample] =
                    activeLaneMask;
                break;

            case SampleMode::Data:
            case SampleMode::InvertedData:
                // Dynamic positions are overwritten on every encode.
                output[base + sample] = 0;
                break;
            }
        }
    }
}

inline std::uint8_t dataPlane(
    const std::uint8_t laneValue[kDataWidth],
    std::uint8_t bit) {

    return static_cast<std::uint8_t>(
        (((laneValue[0] >> bit) & 1U) << 0U) |
        (((laneValue[1] >> bit) & 1U) << 1U) |
        (((laneValue[2] >> bit) & 1U) << 2U) |
        (((laneValue[3] >> bit) & 1U) << 3U) |
        (((laneValue[4] >> bit) & 1U) << 4U) |
        (((laneValue[5] >> bit) & 1U) << 5U) |
        (((laneValue[6] >> bit) & 1U) << 6U) |
        (((laneValue[7] >> bit) & 1U) << 7U));
}

inline void encodeDynamicByte(
    std::uint8_t* output,
    const SamplePlan& plan,
    std::uint8_t activeLaneMask,
    const std::uint8_t laneValue[kDataWidth]) {

    if (output == nullptr ||
        laneValue == nullptr ||
        !plan.valid()) {

        return;
    }

    for (std::uint8_t bitSlot = 0;
         bitSlot < 8;
         ++bitSlot) {

        const std::uint8_t bit =
            static_cast<std::uint8_t>(
                7U -
                bitSlot);

        const std::uint8_t plane =
            static_cast<std::uint8_t>(
                dataPlane(
                    laneValue,
                    bit) &
                activeLaneMask);

        const std::size_t base =
            static_cast<std::size_t>(
                bitSlot) *
            plan.samplesPerBit;

        for (std::uint8_t dynamic = 0;
             dynamic <
                plan.dynamicCount;
             ++dynamic) {

            const std::uint8_t sample =
                plan.dynamicSample[
                    dynamic];

            output[base + sample] =
                plan.dynamicInverted[
                    dynamic]
                    ? static_cast<
                          std::uint8_t>(
                              activeLaneMask ^
                              plane)
                    : plane;
        }
    }
}

} // namespace liteled_parlio
