#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <Preferences.h>

#include "led/LedMappingProfile.h"
#include "render/CorrectionMode.h"
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

    std::uint8_t outputBrightness() const {
        return outputBrightness_;
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

    bool setOutputBrightness(
        std::uint8_t brightness);

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
    static constexpr const char* kOutputBrightnessKey =
        "brightness";
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

    Preferences preferences_;

    CorrectionMode correctionMode_ =
        CorrectionMode::Shadow;

    std::uint8_t outputBrightness_ = 32;

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

    TofSpatialProfile tofSpatialProfile_{};
    bool tofSpatialProfileCustomized_ = false;
    bool tofSpatialProfilePersisted_ = false;

    bool persistenceAvailable_ = false;

    RuntimeSettingsStats stats_{};
};

} // namespace ambilight
