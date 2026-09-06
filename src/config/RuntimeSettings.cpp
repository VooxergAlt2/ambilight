#include "config/RuntimeSettings.h"

#include "config/BoardConfig.h"
#include "config/TofCalibration.h"

#include <algorithm>
#include <cstring>

namespace ambilight {
namespace {

template <std::size_t N>
void copyText(
    std::array<char, N>& destination,
    const char* source) {

    destination.fill('\0');

    if (source == nullptr) {
        return;
    }

    const std::size_t length =
        std::min<std::size_t>(
            std::strlen(source),
            N - 1);

    std::memcpy(
        destination.data(),
        source,
        length);

    destination[length] = '\0';
}

} // namespace

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

    wifiSsid_.fill('\0');
    wifiPassword_.fill('\0');

    tofGainPoints_ =
        config::kTofGainPoints;
    tofGainPointCount_ =
        config::kTofGainPointCount;
    tofGainCurveCustomized_ =
        false;
    tofGainCurvePersisted_ =
        false;

    ledMappingProfile_ = {};
    ledMappingProfileCustomized_ = false;
    ledMappingProfilePersisted_ = false;

    ledPixelMaskProfile_ = {};
    ledPixelMaskProfileCustomized_ = false;
    ledPixelMaskProfilePersisted_ = false;

    tofSpatialProfile_ = {};
    tofSpatialProfileCustomized_ =
        false;
    tofSpatialProfilePersisted_ =
        false;

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
    } else {
        correctionMode_ =
            static_cast<CorrectionMode>(
                raw);
    }

    outputBrightness_ =
        preferences_.getUChar(
            kOutputBrightnessKey,
            config::kDefaultOutputBrightness);

    const String storedSsid =
        preferences_.getString(
            kWifiSsidKey,
            "");

    const String storedPassword =
        preferences_.getString(
            kWifiPasswordKey,
            "");

    const bool wifiStoredValueValid =
        storedSsid.length() <=
            kMaxWifiSsidLength &&
        storedPassword.length() <=
            kMaxWifiPasswordLength;

    if (!wifiStoredValueValid) {
        ++stats_.invalidStoredValues;

        preferences_.remove(
            kWifiSsidKey);

        preferences_.remove(
            kWifiPasswordKey);
    } else if (storedSsid.length() > 0) {
        copyText(
            wifiSsid_,
            storedSsid.c_str());

        copyText(
            wifiPassword_,
            storedPassword.c_str());
    }

    const std::uint16_t ledMapVersion =
        preferences_.getUShort(
            kLedMappingVersionKey,
            0);

    if (ledMapVersion != 0) {
        LedMappingProfile storedMap;

        const std::size_t storedBytes =
            preferences_.getBytesLength(
                kLedMappingProfileKey);

        bool mapValid =
            ledMapVersion ==
                LedMappingProfile::kSchemaVersion &&
            storedBytes ==
                sizeof(LedMappingProfile);

        if (mapValid) {
            const std::size_t loaded =
                preferences_.getBytes(
                    kLedMappingProfileKey,
                    &storedMap,
                    sizeof(storedMap));

            mapValid =
                loaded == sizeof(storedMap) &&
                storedMap.valid();
        }

        if (mapValid) {
            ledMappingProfile_ = storedMap;
            ledMappingProfileCustomized_ = true;
            ledMappingProfilePersisted_ = true;
        } else {
            ++stats_.invalidStoredValues;
            preferences_.remove(kLedMappingProfileKey);
            preferences_.remove(kLedMappingVersionKey);
        }
    }

    const std::uint16_t pixelMaskVersion =
        preferences_.getUShort(
            kLedPixelMaskVersionKey,
            0);

    if (pixelMaskVersion != 0) {
        LedPixelMaskProfile storedMask;

        const std::size_t storedBytes =
            preferences_.getBytesLength(
                kLedPixelMaskProfileKey);

        bool maskValid =
            pixelMaskVersion ==
                LedPixelMaskProfile::kSchemaVersion &&
            storedBytes ==
                sizeof(LedPixelMaskProfile);

        if (maskValid) {
            const std::size_t loaded =
                preferences_.getBytes(
                    kLedPixelMaskProfileKey,
                    &storedMask,
                    sizeof(storedMask));

            maskValid =
                loaded == sizeof(storedMask) &&
                storedMask.valid();
        }

        if (maskValid) {
            ledPixelMaskProfile_ =
                storedMask;

            ledPixelMaskProfileCustomized_ =
                true;

            ledPixelMaskProfilePersisted_ =
                true;
        } else {
            ++stats_.invalidStoredValues;

            preferences_.remove(
                kLedPixelMaskProfileKey);

            preferences_.remove(
                kLedPixelMaskVersionKey);
        }
    }

    const std::uint16_t spatialVersion =
        preferences_.getUShort(
            kTofSpatialVersionKey,
            0);

    if (spatialVersion != 0) {
        TofSpatialProfile storedProfile;

        const std::size_t storedBytes =
            preferences_.getBytesLength(
                kTofSpatialProfileKey);

        bool spatialValid =
            spatialVersion ==
                TofSpatialProfile::kSchemaVersion &&
            storedBytes ==
                sizeof(TofSpatialProfile);

        if (spatialValid) {
            const std::size_t loaded =
                preferences_.getBytes(
                    kTofSpatialProfileKey,
                    &storedProfile,
                    sizeof(storedProfile));

            spatialValid =
                loaded ==
                    sizeof(storedProfile) &&
                storedProfile.valid();
        }

        if (spatialValid) {
            tofSpatialProfile_ =
                storedProfile;

            tofSpatialProfileCustomized_ =
                true;

            tofSpatialProfilePersisted_ =
                true;
        } else {
            ++stats_.invalidStoredValues;

            preferences_.remove(
                kTofSpatialProfileKey);

            preferences_.remove(
                kTofSpatialVersionKey);
        }
    }

    const std::uint8_t storedCurveCount =
        preferences_.getUChar(
            kTofGainCountKey,
            0);

    if (storedCurveCount != 0) {
        std::array<
            GainPoint,
            DistanceGainCurve::kMaxPoints>
            storedPoints{};

        const std::size_t expectedBytes =
            sizeof(storedPoints);

        const std::size_t storedBytes =
            preferences_.getBytesLength(
                kTofGainCurveKey);

        bool storedCurveValid =
            storedCurveCount >= 2 &&
            storedCurveCount <=
                DistanceGainCurve::kMaxPoints &&
            storedBytes ==
                expectedBytes;

        if (storedCurveValid) {
            const std::size_t loaded =
                preferences_.getBytes(
                    kTofGainCurveKey,
                    storedPoints.data(),
                    expectedBytes);

            storedCurveValid =
                loaded == expectedBytes;
        }

        if (storedCurveValid) {
            const DistanceGainCurve curve(
                storedPoints,
                storedCurveCount);

            storedCurveValid =
                curve.valid();
        }

        if (storedCurveValid) {
            tofGainPoints_ =
                storedPoints;

            tofGainPointCount_ =
                storedCurveCount;

            tofGainCurveCustomized_ =
                true;
            tofGainCurvePersisted_ =
                true;
        } else {
            ++stats_.invalidStoredValues;

            preferences_.remove(
                kTofGainCurveKey);

            preferences_.remove(
                kTofGainCountKey);
        }
    }

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


bool RuntimeSettings::setWifiCredentials(
    const char* ssid,
    const char* password) {

    if (ssid == nullptr) {
        return false;
    }

    if (password == nullptr) {
        password = "";
    }

    const std::size_t ssidLength =
        std::strlen(ssid);

    const std::size_t passwordLength =
        std::strlen(password);

    if (ssidLength == 0 ||
        ssidLength > kMaxWifiSsidLength ||
        passwordLength >
            kMaxWifiPasswordLength) {

        return false;
    }

    copyText(
        wifiSsid_,
        ssid);

    copyText(
        wifiPassword_,
        password);

    if (!persistenceAvailable_) {
        ++stats_.writeFailures;
        return false;
    }

    // Write password first and SSID last. SSID acts as the effective commit
    // marker because a missing/empty SSID means "no persisted credentials".
    const std::size_t passwordWritten =
        preferences_.putString(
            kWifiPasswordKey,
            password);

    const std::size_t ssidWritten =
        preferences_.putString(
            kWifiSsidKey,
            ssid);

    const bool passwordOk =
        passwordLength == 0 ||
        passwordWritten > 0;

    const bool ssidOk =
        ssidWritten > 0;

    if (!passwordOk ||
        !ssidOk) {

        ++stats_.writeFailures;

        preferences_.remove(
            kWifiSsidKey);

        preferences_.remove(
            kWifiPasswordKey);

        return false;
    }

    ++stats_.writes;
    return true;
}

bool RuntimeSettings::clearWifiCredentials() {
    wifiSsid_.fill('\0');
    wifiPassword_.fill('\0');

    if (!persistenceAvailable_) {
        ++stats_.writeFailures;
        return false;
    }

    const bool hadSsid =
        preferences_.isKey(
            kWifiSsidKey);

    const bool hadPassword =
        preferences_.isKey(
            kWifiPasswordKey);

    const bool ssidOk =
        !hadSsid ||
        preferences_.remove(
            kWifiSsidKey);

    const bool passwordOk =
        !hadPassword ||
        preferences_.remove(
            kWifiPasswordKey);

    if (!ssidOk ||
        !passwordOk) {

        ++stats_.writeFailures;
        return false;
    }

    if (hadSsid ||
        hadPassword) {
        ++stats_.writes;
    }

    return true;
}


bool RuntimeSettings::setTofGainCurve(
    const std::array<
        GainPoint,
        DistanceGainCurve::kMaxPoints>& points,
    std::size_t count) {

    const DistanceGainCurve curve(
        points,
        count);

    if (!curve.valid()) {
        return false;
    }

    tofGainPoints_ = points;
    tofGainPointCount_ = count;
    tofGainCurveCustomized_ = true;
    tofGainCurvePersisted_ = false;

    if (!persistenceAvailable_) {
        ++stats_.writeFailures;
        return false;
    }

    const std::size_t curveBytes =
        preferences_.putBytes(
            kTofGainCurveKey,
            points.data(),
            sizeof(points));

    const std::size_t countBytes =
        preferences_.putUChar(
            kTofGainCountKey,
            static_cast<std::uint8_t>(
                count));

    if (curveBytes != sizeof(points) ||
        countBytes != sizeof(std::uint8_t)) {

        ++stats_.writeFailures;

        preferences_.remove(
            kTofGainCurveKey);

        preferences_.remove(
            kTofGainCountKey);

        return false;
    }

    tofGainCurvePersisted_ = true;

    ++stats_.writes;
    return true;
}

bool RuntimeSettings::resetTofGainCurve() {
    tofGainPoints_ =
        config::kTofGainPoints;

    tofGainPointCount_ =
        config::kTofGainPointCount;

    tofGainCurveCustomized_ =
        false;
    tofGainCurvePersisted_ =
        false;

    if (!persistenceAvailable_) {
        ++stats_.writeFailures;
        return false;
    }

    const bool hadCurve =
        preferences_.isKey(
            kTofGainCurveKey);

    const bool hadCount =
        preferences_.isKey(
            kTofGainCountKey);

    const bool curveOk =
        !hadCurve ||
        preferences_.remove(
            kTofGainCurveKey);

    const bool countOk =
        !hadCount ||
        preferences_.remove(
            kTofGainCountKey);

    if (!curveOk ||
        !countOk) {

        ++stats_.writeFailures;
        return false;
    }

    if (hadCurve ||
        hadCount) {
        ++stats_.writes;
    }

    return true;
}


bool RuntimeSettings::setTofSpatialProfile(
    const TofSpatialProfile& profile) {

    if (!profile.valid()) {
        return false;
    }

    tofSpatialProfile_ =
        profile;

    tofSpatialProfileCustomized_ =
        true;

    tofSpatialProfilePersisted_ =
        false;

    if (!persistenceAvailable_) {
        ++stats_.writeFailures;
        return false;
    }

    const std::size_t profileBytes =
        preferences_.putBytes(
            kTofSpatialProfileKey,
            &profile,
            sizeof(profile));

    const std::size_t versionBytes =
        preferences_.putUShort(
            kTofSpatialVersionKey,
            TofSpatialProfile::kSchemaVersion);

    if (profileBytes != sizeof(profile) ||
        versionBytes != sizeof(std::uint16_t)) {

        ++stats_.writeFailures;

        preferences_.remove(
            kTofSpatialProfileKey);

        preferences_.remove(
            kTofSpatialVersionKey);

        return false;
    }

    tofSpatialProfilePersisted_ =
        true;

    ++stats_.writes;
    return true;
}

bool RuntimeSettings::resetTofSpatialProfile() {
    tofSpatialProfile_ = {};

    tofSpatialProfileCustomized_ =
        false;

    tofSpatialProfilePersisted_ =
        false;

    if (!persistenceAvailable_) {
        ++stats_.writeFailures;
        return false;
    }

    const bool hadProfile =
        preferences_.isKey(
            kTofSpatialProfileKey);

    const bool hadVersion =
        preferences_.isKey(
            kTofSpatialVersionKey);

    const bool profileOk =
        !hadProfile ||
        preferences_.remove(
            kTofSpatialProfileKey);

    const bool versionOk =
        !hadVersion ||
        preferences_.remove(
            kTofSpatialVersionKey);

    if (!profileOk ||
        !versionOk) {

        ++stats_.writeFailures;
        return false;
    }

    if (hadProfile ||
        hadVersion) {
        ++stats_.writes;
    }

    return true;
}

bool RuntimeSettings::setLedMappingProfile(
    const LedMappingProfile& profile) {

    if (!profile.valid()) {
        return false;
    }

    ledMappingProfile_ = profile;
    ledMappingProfileCustomized_ = true;
    ledMappingProfilePersisted_ = false;

    if (!persistenceAvailable_) {
        ++stats_.writeFailures;
        return false;
    }

    const std::size_t profileBytes =
        preferences_.putBytes(
            kLedMappingProfileKey,
            &profile,
            sizeof(profile));

    const std::size_t versionBytes =
        preferences_.putUShort(
            kLedMappingVersionKey,
            LedMappingProfile::kSchemaVersion);

    if (profileBytes != sizeof(profile) ||
        versionBytes != sizeof(std::uint16_t)) {

        ++stats_.writeFailures;
        preferences_.remove(kLedMappingProfileKey);
        preferences_.remove(kLedMappingVersionKey);
        return false;
    }

    ledMappingProfilePersisted_ = true;
    ++stats_.writes;
    return true;
}

bool RuntimeSettings::resetLedMappingProfile() {
    ledMappingProfile_ = {};
    ledMappingProfileCustomized_ = false;
    ledMappingProfilePersisted_ = false;

    if (!persistenceAvailable_) {
        ++stats_.writeFailures;
        return false;
    }

    const bool hadProfile =
        preferences_.isKey(kLedMappingProfileKey);
    const bool hadVersion =
        preferences_.isKey(kLedMappingVersionKey);

    const bool profileOk =
        !hadProfile ||
        preferences_.remove(kLedMappingProfileKey);
    const bool versionOk =
        !hadVersion ||
        preferences_.remove(kLedMappingVersionKey);

    if (!profileOk || !versionOk) {
        ++stats_.writeFailures;
        return false;
    }

    if (hadProfile || hadVersion) {
        ++stats_.writes;
    }

    return true;
}

bool RuntimeSettings::setLedPixelMaskProfile(
    const LedPixelMaskProfile& profile) {

    if (!profile.valid()) {
        return false;
    }

    ledPixelMaskProfile_ = profile;
    ledPixelMaskProfileCustomized_ = true;
    ledPixelMaskProfilePersisted_ = false;

    if (!persistenceAvailable_) {
        ++stats_.writeFailures;
        return false;
    }

    const std::size_t profileBytes =
        preferences_.putBytes(
            kLedPixelMaskProfileKey,
            &profile,
            sizeof(profile));

    const std::size_t versionBytes =
        preferences_.putUShort(
            kLedPixelMaskVersionKey,
            LedPixelMaskProfile::kSchemaVersion);

    if (profileBytes != sizeof(profile) ||
        versionBytes != sizeof(std::uint16_t)) {

        ++stats_.writeFailures;

        preferences_.remove(
            kLedPixelMaskProfileKey);

        preferences_.remove(
            kLedPixelMaskVersionKey);

        return false;
    }

    ledPixelMaskProfilePersisted_ = true;
    ++stats_.writes;
    return true;
}

bool RuntimeSettings::resetLedPixelMaskProfile() {
    ledPixelMaskProfile_ = {};
    ledPixelMaskProfileCustomized_ = false;
    ledPixelMaskProfilePersisted_ = false;

    if (!persistenceAvailable_) {
        ++stats_.writeFailures;
        return false;
    }

    const bool hadProfile =
        preferences_.isKey(
            kLedPixelMaskProfileKey);

    const bool hadVersion =
        preferences_.isKey(
            kLedPixelMaskVersionKey);

    const bool profileOk =
        !hadProfile ||
        preferences_.remove(
            kLedPixelMaskProfileKey);

    const bool versionOk =
        !hadVersion ||
        preferences_.remove(
            kLedPixelMaskVersionKey);

    if (!profileOk ||
        !versionOk) {

        ++stats_.writeFailures;
        return false;
    }

    if (hadProfile ||
        hadVersion) {
        ++stats_.writes;
    }

    return true;
}

bool RuntimeSettings::factoryReset() {
    if (!persistenceAvailable_) {
        ++stats_.writeFailures;
        return false;
    }

    // Durable clear happens first. If it fails, leave the live runtime view
    // untouched so the caller can continue safely without a partial reset.
    if (!preferences_.clear()) {
        ++stats_.writeFailures;
        return false;
    }

    correctionMode_ =
        CorrectionMode::Shadow;

    outputBrightness_ =
        config::kDefaultOutputBrightness;

    wifiSsid_.fill('\0');
    wifiPassword_.fill('\0');

    tofGainPoints_ =
        config::kTofGainPoints;
    tofGainPointCount_ =
        config::kTofGainPointCount;
    tofGainCurveCustomized_ =
        false;
    tofGainCurvePersisted_ =
        false;

    tofSpatialProfile_ = {};
    tofSpatialProfileCustomized_ =
        false;
    tofSpatialProfilePersisted_ =
        false;

    ledMappingProfile_ = {};
    ledMappingProfileCustomized_ =
        false;
    ledMappingProfilePersisted_ =
        false;

    ledPixelMaskProfile_ = {};
    ledPixelMaskProfileCustomized_ =
        false;
    ledPixelMaskProfilePersisted_ =
        false;

    ++stats_.writes;
    return true;
}

} // namespace ambilight
