#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <Preferences.h>

#include "led/LedMappingProfile.h"
#include "led/LedPixelMaskProfile.h"
#include "render/CorrectionMode.h"
#include "render/ManualLighting.h"
#include "tof/TofGainModel.h"
#include "tof/TofSpatialProfile.h"

namespace ambilight {

struct RuntimeSettingsStats {
    std::uint32_t writes = 0;
    std::uint32_t writeFailures = 0;
    std::uint32_t invalidStoredValues = 0;
};

class RuntimeSettings {
public:
    static constexpr std::size_t kMaxWifiSsidLength = 32;
    static constexpr std::size_t kMaxWifiPasswordLength = 63;
    RuntimeSettings() = default;
    ~RuntimeSettings();

    RuntimeSettings(const RuntimeSettings&) = delete;
    RuntimeSettings& operator=(const RuntimeSettings&) = delete;

    bool begin();

    CorrectionMode correctionMode() const {
        return correctionMode_;
    }

    // Compatibility alias for pre-split callers. New code should select the
    // brightness bank explicitly from the active/selected source.
    std::uint8_t outputBrightness() const {
        return ddpBrightness_;
    }

    std::uint8_t ddpBrightness() const {
        return ddpBrightness_;
    }

    std::uint8_t lightingBrightness() const {
        return lightingBrightness_;
    }

    const ManualLightingState& manualLightingState() const {
        return manualLightingState_;
    }

    bool outputEnabled() const {
        return outputEnabled_;
    }

    bool wifiCredentialsPresent() const {
        return wifiSsid_[0] != '\0';
    }

    const char* wifiSsid() const {
        return wifiSsid_.data();
    }

    const char* wifiPassword() const {
        return wifiPassword_.data();
    }

    const std::array<
        GainPoint,
        DistanceGainCurve::kMaxPoints>&
    tofGainPoints() const {
        return tofGainPoints_;
    }

    std::size_t tofGainPointCount() const {
        return tofGainPointCount_;
    }

    bool tofGainCurveCustomized() const {
        return tofGainCurveCustomized_;
    }

    bool tofGainCurvePersisted() const {
        return tofGainCurvePersisted_;
    }

    DistanceGainCurve tofGainCurve() const {
        return DistanceGainCurve(
            tofGainPoints_,
            tofGainPointCount_);
    }

    const LedMappingProfile& ledMappingProfile() const {
        return ledMappingProfile_;
    }

    bool ledMappingProfileCustomized() const {
        return ledMappingProfileCustomized_;
    }

    bool ledMappingProfilePersisted() const {
        return ledMappingProfilePersisted_;
    }

    const LedPixelMaskProfile& ledPixelMaskProfile() const {
        return ledPixelMaskProfile_;
    }

    bool ledPixelMaskProfileCustomized() const {
        return ledPixelMaskProfileCustomized_;
    }

    bool ledPixelMaskProfilePersisted() const {
        return ledPixelMaskProfilePersisted_;
    }

    const TofSpatialProfile& tofSpatialProfile() const {
        return tofSpatialProfile_;
    }

    bool tofSpatialProfileCustomized() const {
        return tofSpatialProfileCustomized_;
    }

    bool tofSpatialProfilePersisted() const {
        return tofSpatialProfilePersisted_;
    }

    // Runtime state changes even if persistence is unavailable. Return value
    // reports whether the new value is durably stored.
    bool setCorrectionMode(
        CorrectionMode mode);

    bool setOutputState(
        bool enabled,
        std::uint8_t ddpBrightness,
        std::uint8_t lightingBrightness);

    // Compatibility transaction: applies one value to both banks.
    bool setOutputState(
        bool enabled,
        std::uint8_t brightness);

    // Compatibility setter used by the legacy serial/API surface. It updates
    // both banks atomically so old clients preserve their historical meaning.
    bool setOutputBrightness(
        std::uint8_t brightness);

    bool setDdpBrightness(
        std::uint8_t brightness);

    bool setLightingBrightness(
        std::uint8_t brightness);

    bool setOutputEnabled(
        bool enabled);

    bool setManualLightingState(
        const ManualLightingState& state);

    bool setWifiCredentials(
        const char* ssid,
        const char* password);

    bool clearWifiCredentials();

    bool setTofGainCurve(
        const std::array<
            GainPoint,
            DistanceGainCurve::kMaxPoints>& points,
        std::size_t count);

    bool resetTofGainCurve();

    bool setTofSpatialProfile(
        const TofSpatialProfile& profile);

    bool resetTofSpatialProfile();

    bool setLedMappingProfile(
        const LedMappingProfile& profile);

    bool resetLedMappingProfile();

    bool setLedPixelMaskProfile(
        const LedPixelMaskProfile& profile);

    // Adopt a mask that has already been applied safely to the live renderer
    // but could not be committed to NVS. This is intentionally separate from
    // the normal transactional setter and is used only by topology-driven
    // sanitization so RuntimeSettings cannot disagree with active hardware.
    bool adoptLedPixelMaskProfileRuntime(
        const LedPixelMaskProfile& profile);

    bool resetLedPixelMaskProfile();

    // Clear the complete ambilight NVS namespace and restore the in-memory
    // runtime view to firmware defaults. Return value reports durable clear.
    bool factoryReset();

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
    static constexpr const char* kOutputStateKey =
        "output_state";
    static constexpr const char* kManualLightingKey =
        "manual_light";

    // Migration-only keys from Stage <=45 and early Stage 46 builds.
    static constexpr const char* kLegacyOutputBrightnessKey =
        "brightness";
    static constexpr const char* kLegacyOutputEnabledKey =
        "output_on";
    static constexpr const char* kWifiSsidKey =
        "wifi_ssid";
    static constexpr const char* kWifiPasswordKey =
        "wifi_password";
    static constexpr const char* kTofGainCurveKey =
        "tof_curve";
    static constexpr const char* kTofGainCountKey =
        "tof_count";
    static constexpr const char* kTofSpatialProfileKey =
        "spatial";
    static constexpr const char* kTofSpatialVersionKey =
        "spatial_ver";
    static constexpr const char* kLedMappingProfileKey =
        "led_map";
    static constexpr const char* kLedMappingVersionKey =
        "led_map_ver";
    static constexpr const char* kLedPixelMaskProfileKey =
        "pixel_mask";
    static constexpr const char* kLedPixelMaskVersionKey =
        "pixel_mask_ver";

    Preferences preferences_;

    CorrectionMode correctionMode_ =
        CorrectionMode::Shadow;

    std::uint8_t ddpBrightness_ = 32;
    std::uint8_t lightingBrightness_ = 32;
    bool outputEnabled_ = true;
    bool outputStatePersisted_ = false;

    ManualLightingState manualLightingState_{};
    bool manualLightingStatePersisted_ = false;

    std::array<
        char,
        kMaxWifiSsidLength + 1>
        wifiSsid_{};

    std::array<
        char,
        kMaxWifiPasswordLength + 1>
        wifiPassword_{};

    std::array<
        GainPoint,
        DistanceGainCurve::kMaxPoints>
        tofGainPoints_{};

    std::size_t tofGainPointCount_ = 0;
    bool tofGainCurveCustomized_ = false;
    bool tofGainCurvePersisted_ = false;

    LedMappingProfile ledMappingProfile_{};
    bool ledMappingProfileCustomized_ = false;
    bool ledMappingProfilePersisted_ = false;

    LedPixelMaskProfile ledPixelMaskProfile_{};
    bool ledPixelMaskProfileCustomized_ = false;
    bool ledPixelMaskProfilePersisted_ = false;

    TofSpatialProfile tofSpatialProfile_{};
    bool tofSpatialProfileCustomized_ = false;
    bool tofSpatialProfilePersisted_ = false;

    bool persistenceAvailable_ = false;

    RuntimeSettingsStats stats_{};
};

} // namespace ambilight
