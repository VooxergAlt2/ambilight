#include "config/RuntimeSettings.h"

#include "config/BoardConfig.h"

namespace ambilight {

RuntimeSettings::~RuntimeSettings() {
    if (persistenceAvailable_) {
        preferences_.end();
    }
}

bool RuntimeSettings::begin() {
    correctionMode_ =
        CorrectionMode::Shadow;

    outputBrightness_ =
        config::kDefaultOutputBrightness;

    persistenceAvailable_ =
        preferences_.begin(
            kNamespace,
            false);

    if (!persistenceAvailable_) {
        return false;
    }

    const std::uint8_t fallback =
        static_cast<std::uint8_t>(
            CorrectionMode::Shadow);

    const std::uint8_t raw =
        preferences_.getUChar(
            kCorrectionModeKey,
            fallback);

    if (!correctionModeValid(raw)) {
        ++stats_.invalidStoredValues;

        const std::size_t written =
            preferences_.putUChar(
                kCorrectionModeKey,
                fallback);

        if (written == sizeof(std::uint8_t)) {
            ++stats_.writes;
        } else {
            ++stats_.writeFailures;
        }

        correctionMode_ =
            CorrectionMode::Shadow;

        return true;
    }

    correctionMode_ =
        static_cast<CorrectionMode>(
            raw);

    outputBrightness_ =
        preferences_.getUChar(
            kOutputBrightnessKey,
            config::kDefaultOutputBrightness);

    return true;
}

bool RuntimeSettings::setCorrectionMode(
    CorrectionMode mode) {

    const std::uint8_t raw =
        static_cast<std::uint8_t>(
            mode);

    if (!correctionModeValid(raw)) {
        return false;
    }

    if (correctionMode_ == mode) {
        return persistenceAvailable_;
    }

    correctionMode_ = mode;

    if (!persistenceAvailable_) {
        ++stats_.writeFailures;
        return false;
    }

    const std::size_t written =
        preferences_.putUChar(
            kCorrectionModeKey,
            raw);

    if (written != sizeof(std::uint8_t)) {
        ++stats_.writeFailures;
        return false;
    }

    ++stats_.writes;
    return true;
}

bool RuntimeSettings::setOutputBrightness(
    std::uint8_t brightness) {

    if (outputBrightness_ ==
        brightness) {
        return persistenceAvailable_;
    }

    outputBrightness_ =
        brightness;

    if (!persistenceAvailable_) {
        ++stats_.writeFailures;
        return false;
    }

    const std::size_t written =
        preferences_.putUChar(
            kOutputBrightnessKey,
            brightness);

    if (written != sizeof(std::uint8_t)) {
        ++stats_.writeFailures;
        return false;
    }

    ++stats_.writes;
    return true;
}

} // namespace ambilight
