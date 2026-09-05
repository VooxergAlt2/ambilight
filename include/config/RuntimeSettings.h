#pragma once

#include <cstdint>

#include <Preferences.h>

#include "render/CorrectionMode.h"

namespace ambilight {

struct RuntimeSettingsStats {
    std::uint32_t writes = 0;
    std::uint32_t writeFailures = 0;
    std::uint32_t invalidStoredValues = 0;
};

class RuntimeSettings {
public:
    RuntimeSettings() = default;
    ~RuntimeSettings();

    RuntimeSettings(const RuntimeSettings&) = delete;
    RuntimeSettings& operator=(const RuntimeSettings&) = delete;

    bool begin();

    CorrectionMode correctionMode() const {
        return correctionMode_;
    }

    std::uint8_t outputBrightness() const {
        return outputBrightness_;
    }

    // Runtime state changes even if persistence is unavailable. Return value
    // reports whether the new value is durably stored.
    bool setCorrectionMode(
        CorrectionMode mode);

    bool setOutputBrightness(
        std::uint8_t brightness);

    bool persistenceAvailable() const {
        return persistenceAvailable_;
    }

    const RuntimeSettingsStats& stats() const {
        return stats_;
    }

private:
    static constexpr const char* kNamespace =
        "ambilight";
    static constexpr const char* kCorrectionModeKey =
        "corr_mode";
    static constexpr const char* kOutputBrightnessKey =
        "brightness";

    Preferences preferences_;

    CorrectionMode correctionMode_ =
        CorrectionMode::Shadow;

    std::uint8_t outputBrightness_ = 32;

    bool persistenceAvailable_ = false;

    RuntimeSettingsStats stats_{};
};

} // namespace ambilight
