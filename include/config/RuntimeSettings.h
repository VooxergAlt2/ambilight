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

    // Runtime state changes even if persistence is unavailable. Return value
    // reports whether the new value is durably stored.
    bool setCorrectionMode(
        CorrectionMode mode);

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

    Preferences preferences_;

    CorrectionMode correctionMode_ =
        CorrectionMode::Shadow;

    bool persistenceAvailable_ = false;

    RuntimeSettingsStats stats_{};
};

} // namespace ambilight
