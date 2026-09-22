#include "config/RuntimeSettings.h"

#include "config/BoardConfig.h"
#include "config/TofCalibration.h"

#include <algorithm>
#include <cstring>

namespace ambilight {
namespace {

struct OutputStateRecord {
    static constexpr std::uint16_t kSchemaVersion = 1;

    std::uint16_t schemaVersion =
        kSchemaVersion;

    std::uint8_t brightness =
        config::kDefaultOutputBrightness;

    std::uint8_t enabled = 1;
};

static_assert(
    sizeof(OutputStateRecord) == 4,
    "OutputStateRecord persistence layout must stay stable");

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

    outputEnabled_ = true;
    outputStatePersisted_ = false;

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
        // Never destructively self-heal persisted settings during boot. The
        // value may belong to another firmware version, so keep it intact for
        // downgrade/recovery and use a safe runtime fallback only.
        correctionMode_ =
            CorrectionMode::Shadow;
    } else {
        correctionMode_ =
            static_cast<CorrectionMode>(
                raw);
    }

    const std::size_t outputStateBytes =
        preferences_.getBytesLength(
            kOutputStateKey);

    bool outputStateLoaded = false;

    if (outputStateBytes != 0) {
        OutputStateRecord stored{};

        const bool shapeValid =
            outputStateBytes ==
                sizeof(stored) &&
            preferences_.getBytes(
                kOutputStateKey,
                &stored,
                sizeof(stored)) ==
                    sizeof(stored);

        const bool valueValid =
            shapeValid &&
            stored.schemaVersion ==
                OutputStateRecord::
                    kSchemaVersion &&
            stored.enabled <= 1U;

        if (valueValid) {
            outputBrightness_ =
                stored.brightness;

            outputEnabled_ =
                stored.enabled != 0U;

            outputStateLoaded = true;
            outputStatePersisted_ = true;
        } else {
            ++stats_.invalidStoredValues;
        }
    }

    if (!outputStateLoaded &&
        outputStateBytes == 0) {
        // Migrate the pre-Stage-46 representation. Stage <=45 encoded power
        // only through brightness=0; early Stage 46 builds added output_on as
        // a second key. The new record stores both values atomically.
        outputBrightness_ =
            preferences_.getUChar(
                kLegacyOutputBrightnessKey,
                config::kDefaultOutputBrightness);

        const bool legacyEnabledStored =
            preferences_.isKey(
                kLegacyOutputEnabledKey);

        const std::uint8_t legacyEnabled =
            preferences_.getUChar(
                kLegacyOutputEnabledKey,
                outputBrightness_ != 0
                    ? 1U
                    : 0U);

        if (legacyEnabledStored &&
            legacyEnabled > 1U) {

            ++stats_.invalidStoredValues;
        }

        outputEnabled_ =
            legacyEnabledStored &&
            legacyEnabled <= 1U
                ? legacyEnabled != 0U
                : outputBrightness_ != 0;

        OutputStateRecord migrated{};
        migrated.brightness =
            outputBrightness_;
        migrated.enabled =
            outputEnabled_
                ? 1U
                : 0U;

        const std::size_t written =
            preferences_.putBytes(
                kOutputStateKey,
                &migrated,
                sizeof(migrated));

        if (written == sizeof(migrated)) {
            outputStatePersisted_ = true;
            ++stats_.writes;

            // Legacy values are now inert. Remove them only after the new
            // single-record commit succeeded so a failed migration remains
            // recoverable on the next boot.
            preferences_.remove(
                kLegacyOutputBrightnessKey);

            preferences_.remove(
                kLegacyOutputEnabledKey);
        } else {
            ++stats_.writeFailures;
        }
    }

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

        bool maskAdjustedForTopology = false;

        if (maskValid) {
            const std::size_t loaded =
                preferences_.getBytes(
                    kLedPixelMaskProfileKey,
                    &storedMask,
                    sizeof(storedMask));

            maskValid =
                loaded == sizeof(storedMask) &&
                storedMask.valid();

            if (maskValid &&
                !storedMask.validFor(
                    ledMappingProfile_)) {

                // Keep the original NVS record intact. A topology from a
                // different firmware version may make the mask meaningful
                // again after downgrade/recovery. Only the live view is
                // sanitized for the active topology.
                storedMask.sanitizeFor(
                    ledMappingProfile_);
                maskAdjustedForTopology = true;
            }
        }

        if (maskValid) {
            ledPixelMaskProfile_ =
                storedMask;

            ledPixelMaskProfileCustomized_ =
                true;

            ledPixelMaskProfilePersisted_ =
                !maskAdjustedForTopology;
        } else {
            ++stats_.invalidStoredValues;
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

bool RuntimeSettings::setOutputState(
    bool enabled,
    std::uint8_t brightness) {

    const bool stateChanged =
        outputEnabled_ != enabled ||
        outputBrightness_ != brightness;

    if (!stateChanged &&
        outputStatePersisted_) {

        return true;
    }

    // Runtime control remains available even when NVS is unavailable or a
    // write fails. Keep a separate persistence flag so a later identical
    // request retries a previously failed durable commit instead of reporting
    // a false success merely because Preferences is available.
    outputEnabled_ = enabled;
    outputBrightness_ = brightness;

    if (!persistenceAvailable_) {
        outputStatePersisted_ = false;
        ++stats_.writeFailures;
        return false;
    }

    OutputStateRecord record{};
    record.brightness =
        outputBrightness_;

    record.enabled =
        outputEnabled_
            ? 1U
            : 0U;

    const std::size_t written =
        preferences_.putBytes(
            kOutputStateKey,
            &record,
            sizeof(record));

    if (written != sizeof(record)) {
        outputStatePersisted_ = false;
        ++stats_.writeFailures;
        return false;
    }

    outputStatePersisted_ = true;
    ++stats_.writes;
    return true;
}

bool RuntimeSettings::setOutputBrightness(
    std::uint8_t brightness) {

    return
        setOutputState(
            outputEnabled_,
            brightness);
}

bool RuntimeSettings::setOutputEnabled(
    bool enabled) {

    return
        setOutputState(
            enabled,
            outputBrightness_);
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
    if (!persistenceAvailable_) {
        wifiSsid_.fill('\0');
        wifiPassword_.fill('\0');

        ++stats_.writeFailures;
        return false;
    }

    const bool hadSsid =
        preferences_.isKey(
            kWifiSsidKey);

    const bool hadPassword =
        preferences_.isKey(
            kWifiPasswordKey);

    // SSID is the effective commit marker. Remove it before touching live
    // credentials so a failed durable clear cannot silently return after
    // reboot.
    const bool ssidOk =
        !hadSsid ||
        preferences_.remove(
            kWifiSsidKey);

    if (!ssidOk) {
        ++stats_.writeFailures;
        return false;
    }

    const bool passwordOk =
        !hadPassword ||
        preferences_.remove(
            kWifiPasswordKey);

    if (!passwordOk) {
        // Without SSID the leftover password is inert.
        ++stats_.writeFailures;
    }

    wifiSsid_.fill('\0');
    wifiPassword_.fill('\0');

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
    if (!persistenceAvailable_) {
        tofGainPoints_ =
            config::kTofGainPoints;

        tofGainPointCount_ =
            config::kTofGainPointCount;

        tofGainCurveCustomized_ =
            false;
        tofGainCurvePersisted_ =
            false;

        ++stats_.writeFailures;
        return false;
    }

    const bool hadCurve =
        preferences_.isKey(
            kTofGainCurveKey);

    const bool hadCount =
        preferences_.isKey(
            kTofGainCountKey);

    // Count is the curve commit marker. Remove it first so a stale data blob
    // cannot become active again after a reset.
    const bool countOk =
        !hadCount ||
        preferences_.remove(
            kTofGainCountKey);

    if (!countOk) {
        ++stats_.writeFailures;
        return false;
    }

    const bool curveOk =
        !hadCurve ||
        preferences_.remove(
            kTofGainCurveKey);

    if (!curveOk) {
        ++stats_.writeFailures;
    }

    tofGainPoints_ =
        config::kTofGainPoints;

    tofGainPointCount_ =
        config::kTofGainPointCount;

    tofGainCurveCustomized_ =
        false;
    tofGainCurvePersisted_ =
        false;

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
    if (!persistenceAvailable_) {
        tofSpatialProfile_ = {};

        tofSpatialProfileCustomized_ =
            false;

        tofSpatialProfilePersisted_ =
            false;

        ++stats_.writeFailures;
        return false;
    }

    const bool hadProfile =
        preferences_.isKey(
            kTofSpatialProfileKey);

    const bool hadVersion =
        preferences_.isKey(
            kTofSpatialVersionKey);

    // Version is the commit marker. Remove it before live state changes.
    const bool versionOk =
        !hadVersion ||
        preferences_.remove(
            kTofSpatialVersionKey);

    if (!versionOk) {
        ++stats_.writeFailures;
        return false;
    }

    const bool profileOk =
        !hadProfile ||
        preferences_.remove(
            kTofSpatialProfileKey);

    if (!profileOk) {
        ++stats_.writeFailures;
    }

    tofSpatialProfile_ = {};

    tofSpatialProfileCustomized_ =
        false;

    tofSpatialProfilePersisted_ =
        false;

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

    if (!persistenceAvailable_) {
        // Preserve the existing runtime-only capability when NVS is
        // unavailable, but never claim durable persistence.
        ledMappingProfile_ = profile;
        ledMappingProfileCustomized_ = true;
        ledMappingProfilePersisted_ = false;

        ++stats_.writeFailures;
        return false;
    }

    // With NVS available, persistence is the commit point. Do not mutate the
    // live RuntimeSettings profile until both blob and schema marker are
    // durably written, otherwise a failed save leaves RAM and reboot state
    // disagreeing.
    const std::size_t profileBytes =
        preferences_.putBytes(
            kLedMappingProfileKey,
            &profile,
            sizeof(profile));

    if (profileBytes != sizeof(profile)) {
        ++stats_.writeFailures;

        preferences_.remove(
            kLedMappingProfileKey);

        preferences_.remove(
            kLedMappingVersionKey);

        ledMappingProfilePersisted_ = false;
        return false;
    }

    const std::size_t versionBytes =
        preferences_.putUShort(
            kLedMappingVersionKey,
            LedMappingProfile::kSchemaVersion);

    if (versionBytes != sizeof(std::uint16_t)) {
        ++stats_.writeFailures;

        preferences_.remove(
            kLedMappingProfileKey);

        preferences_.remove(
            kLedMappingVersionKey);

        ledMappingProfilePersisted_ = false;
        return false;
    }

    ledMappingProfile_ = profile;
    ledMappingProfileCustomized_ = true;
    ledMappingProfilePersisted_ = true;

    ++stats_.writes;
    return true;
}

bool RuntimeSettings::resetLedMappingProfile() {
    if (!persistenceAvailable_) {
        ledMappingProfile_ = {};
        ledMappingProfileCustomized_ = false;
        ledMappingProfilePersisted_ = false;

        ++stats_.writeFailures;
        return false;
    }

    const bool hadProfile =
        preferences_.isKey(kLedMappingProfileKey);
    const bool hadVersion =
        preferences_.isKey(kLedMappingVersionKey);

    // Version is the commit marker. Remove it first so a later failure while
    // cleaning the data blob can never resurrect the old topology on reboot.
    const bool versionOk =
        !hadVersion ||
        preferences_.remove(kLedMappingVersionKey);

    if (!versionOk) {
        ++stats_.writeFailures;
        return false;
    }

    const bool profileOk =
        !hadProfile ||
        preferences_.remove(kLedMappingProfileKey);

    if (!profileOk) {
        // The stale blob is now inert because its version marker is gone.
        ++stats_.writeFailures;
    }

    ledMappingProfile_ = {};
    ledMappingProfileCustomized_ = false;
    ledMappingProfilePersisted_ = false;

    if (hadProfile || hadVersion) {
        ++stats_.writes;
    }

    return true;
}

bool RuntimeSettings::setLedPixelMaskProfile(
    const LedPixelMaskProfile& profile) {

    if (!profile.validFor(
            ledMappingProfile_)) {

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
    if (!persistenceAvailable_) {
        ledPixelMaskProfile_ = {};
        ledPixelMaskProfileCustomized_ = false;
        ledPixelMaskProfilePersisted_ = false;

        ++stats_.writeFailures;
        return false;
    }

    const bool hadProfile =
        preferences_.isKey(
            kLedPixelMaskProfileKey);

    const bool hadVersion =
        preferences_.isKey(
            kLedPixelMaskVersionKey);

    // Remove the commit marker first. A leftover data blob without its
    // schema/version marker is ignored at boot and therefore cannot restore
    // a mask that the operator already reset.
    const bool versionOk =
        !hadVersion ||
        preferences_.remove(
            kLedPixelMaskVersionKey);

    if (!versionOk) {
        ++stats_.writeFailures;
        return false;
    }

    const bool profileOk =
        !hadProfile ||
        preferences_.remove(
            kLedPixelMaskProfileKey);

    if (!profileOk) {
        ++stats_.writeFailures;
    }

    ledPixelMaskProfile_ = {};
    ledPixelMaskProfileCustomized_ = false;
    ledPixelMaskProfilePersisted_ = false;

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

    outputEnabled_ = true;
    outputStatePersisted_ = false;

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
