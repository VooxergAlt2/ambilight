#include "config/RuntimeSettings.h"

#include "config/BoardConfig.h"
#include "config/TofCalibration.h"

#include <algorithm>
#include <cstring>

namespace ambilight {
namespace {

struct OutputStateRecordV1 {
    static constexpr std::uint16_t kSchemaVersion = 1;

    std::uint16_t schemaVersion =
        kSchemaVersion;

    std::uint8_t brightness =
        config::kDefaultOutputBrightness;

    std::uint8_t enabled = 1;
};

static_assert(
    sizeof(OutputStateRecordV1) == 4,
    "OutputStateRecordV1 persistence layout must stay stable");

struct OutputStateRecord {
    static constexpr std::uint16_t kSchemaVersion = 2;

    std::uint16_t schemaVersion =
        kSchemaVersion;

    std::uint8_t ddpBrightness =
        config::kDefaultOutputBrightness;

    std::uint8_t lightingBrightness =
        config::kDefaultOutputBrightness;

    std::uint8_t enabled = 1;
    std::uint8_t reserved = 0;
};

static_assert(
    sizeof(OutputStateRecord) == 6,
    "OutputStateRecord persistence layout must stay stable");

struct ManualLightingRecord {
    static constexpr std::uint16_t kSchemaVersion = 1;

    std::uint16_t schemaVersion =
        kSchemaVersion;

    std::uint8_t effect =
        static_cast<std::uint8_t>(
            ManualLightingEffect::Ambilight);

    std::uint8_t fallbackEffect =
        static_cast<std::uint8_t>(
            ManualLightingEffect::BiasWhite);

    std::uint8_t red = 255;
    std::uint8_t green = 255;
    std::uint8_t blue = 255;
    std::uint8_t speed = 128;
    std::uint8_t intensity = 128;
    std::uint8_t reserved = 0;
};

static_assert(
    sizeof(ManualLightingRecord) == 10,
    "ManualLightingRecord persistence layout must stay stable");

bool manualLightingRecordValid(
    const ManualLightingRecord& record) {

    return
        record.schemaVersion ==
            ManualLightingRecord::kSchemaVersion &&
        ManualLighting::validEffect(
            record.effect) &&
        ManualLighting::validLocalEffect(
            static_cast<ManualLightingEffect>(
                record.fallbackEffect));
}

ManualLightingState decodeManualLightingRecord(
    const ManualLightingRecord& record) {

    ManualLightingState state;
    state.effect =
        static_cast<ManualLightingEffect>(
            record.effect);
    state.fallbackEffect =
        static_cast<ManualLightingEffect>(
            record.fallbackEffect);
    state.color = {
        record.red,
        record.green,
        record.blue
    };
    state.speed = record.speed;
    state.intensity = record.intensity;
    return state;
}

ManualLightingRecord encodeManualLightingRecord(
    const ManualLightingState& state) {

    ManualLightingRecord record;
    record.effect =
        static_cast<std::uint8_t>(
            state.effect);
    record.fallbackEffect =
        static_cast<std::uint8_t>(
            state.fallbackEffect);
    record.red = state.color.r;
    record.green = state.color.g;
    record.blue = state.color.b;
    record.speed = state.speed;
    record.intensity = state.intensity;
    return record;
}

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

    ddpBrightness_ =
        config::kDefaultOutputBrightness;

    lightingBrightness_ =
        config::kDefaultOutputBrightness;

    outputEnabled_ = true;
    outputStatePersisted_ = false;

    manualLightingState_ = {};
    manualLightingStatePersisted_ = false;

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

    if (outputStateBytes ==
        sizeof(OutputStateRecord)) {

        OutputStateRecord stored{};

        const bool shapeValid =
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
            ddpBrightness_ =
                stored.ddpBrightness;
            lightingBrightness_ =
                stored.lightingBrightness;
            outputEnabled_ =
                stored.enabled != 0U;
            outputStateLoaded = true;
            outputStatePersisted_ = true;
        } else {
            ++stats_.invalidStoredValues;
        }
    } else if (outputStateBytes ==
               sizeof(OutputStateRecordV1)) {

        OutputStateRecordV1 stored{};

        const bool valueValid =
            preferences_.getBytes(
                kOutputStateKey,
                &stored,
                sizeof(stored)) ==
                    sizeof(stored) &&
            stored.schemaVersion ==
                OutputStateRecordV1::
                    kSchemaVersion &&
            stored.enabled <= 1U;

        if (valueValid) {
            // v1 had a single brightness bank. Copy it into both new banks so
            // an upgrade is visually identical until the user changes them.
            ddpBrightness_ =
                stored.brightness;
            lightingBrightness_ =
                stored.brightness;
            outputEnabled_ =
                stored.enabled != 0U;
            outputStateLoaded = true;

            OutputStateRecord migrated{};
            migrated.ddpBrightness =
                ddpBrightness_;
            migrated.lightingBrightness =
                lightingBrightness_;
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
            } else {
                outputStatePersisted_ = false;
                ++stats_.writeFailures;
            }
        } else {
            // A same-sized future/corrupt record must not be overwritten by
            // legacy migration. Keep the raw bytes for downgrade/recovery.
            ++stats_.invalidStoredValues;
        }
    } else if (outputStateBytes != 0) {
        // Unknown future shape. Never self-heal destructively on boot.
        ++stats_.invalidStoredValues;
    }

    if (!outputStateLoaded &&
        outputStateBytes == 0) {
        // Migrate the pre-Stage-46 representation. Stage <=45 encoded power
        // only through brightness=0; early Stage 46 builds added output_on as
        // a second key. The new record stores power and both brightness banks
        // atomically.
        const std::uint8_t legacyBrightness =
            preferences_.getUChar(
                kLegacyOutputBrightnessKey,
                config::kDefaultOutputBrightness);

        ddpBrightness_ = legacyBrightness;
        lightingBrightness_ = legacyBrightness;

        const bool legacyEnabledStored =
            preferences_.isKey(
                kLegacyOutputEnabledKey);

        const std::uint8_t legacyEnabled =
            preferences_.getUChar(
                kLegacyOutputEnabledKey,
                legacyBrightness != 0
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
                : legacyBrightness != 0;

        OutputStateRecord migrated{};
        migrated.ddpBrightness =
            ddpBrightness_;
        migrated.lightingBrightness =
            lightingBrightness_;
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

            preferences_.remove(
                kLegacyOutputBrightnessKey);
            preferences_.remove(
                kLegacyOutputEnabledKey);
        } else {
            ++stats_.writeFailures;
        }
    }

    const std::size_t manualLightingBytes =
        preferences_.getBytesLength(
            kManualLightingKey);

    if (manualLightingBytes != 0) {
        ManualLightingRecord stored{};

        const bool valid =
            manualLightingBytes ==
                sizeof(stored) &&
            preferences_.getBytes(
                kManualLightingKey,
                &stored,
                sizeof(stored)) ==
                    sizeof(stored) &&
            manualLightingRecordValid(
                stored);

        if (valid) {
            manualLightingState_ =
                decodeManualLightingRecord(
                    stored);
            manualLightingStatePersisted_ = true;
        } else {
            // Preserve unknown/corrupt records in NVS; use safe defaults only
            // in the live view so downgrade/recovery remains possible.
            ++stats_.invalidStoredValues;
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
    std::uint8_t ddpBrightness,
    std::uint8_t lightingBrightness) {

    const bool stateChanged =
        outputEnabled_ != enabled ||
        ddpBrightness_ != ddpBrightness ||
        lightingBrightness_ != lightingBrightness;

    if (!stateChanged &&
        outputStatePersisted_) {

        return true;
    }

    outputEnabled_ = enabled;
    ddpBrightness_ = ddpBrightness;
    lightingBrightness_ = lightingBrightness;

    if (!persistenceAvailable_) {
        outputStatePersisted_ = false;
        ++stats_.writeFailures;
        return false;
    }

    OutputStateRecord record{};
    record.ddpBrightness =
        ddpBrightness_;
    record.lightingBrightness =
        lightingBrightness_;
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

bool RuntimeSettings::setOutputState(
    bool enabled,
    std::uint8_t brightness) {

    return
        setOutputState(
            enabled,
            brightness,
            brightness);
}

bool RuntimeSettings::setOutputBrightness(
    std::uint8_t brightness) {

    return
        setOutputState(
            outputEnabled_,
            brightness,
            brightness);
}

bool RuntimeSettings::setDdpBrightness(
    std::uint8_t brightness) {

    return
        setOutputState(
            outputEnabled_,
            brightness,
            lightingBrightness_);
}

bool RuntimeSettings::setLightingBrightness(
    std::uint8_t brightness) {

    return
        setOutputState(
            outputEnabled_,
            ddpBrightness_,
            brightness);
}

bool RuntimeSettings::setOutputEnabled(
    bool enabled) {

    return
        setOutputState(
            enabled,
            ddpBrightness_,
            lightingBrightness_);
}

bool RuntimeSettings::setManualLightingState(
    const ManualLightingState& state) {

    if (!ManualLighting::validEffect(
            static_cast<std::uint8_t>(
                state.effect)) ||
        !ManualLighting::validLocalEffect(
            state.fallbackEffect)) {

        return false;
    }

    const bool changed =
        manualLightingState_.effect != state.effect ||
        manualLightingState_.fallbackEffect !=
            state.fallbackEffect ||
        !(manualLightingState_.color == state.color) ||
        manualLightingState_.speed != state.speed ||
        manualLightingState_.intensity != state.intensity;

    if (!changed &&
        manualLightingStatePersisted_) {

        return true;
    }

    manualLightingState_ = state;

    if (!persistenceAvailable_) {
        manualLightingStatePersisted_ = false;
        ++stats_.writeFailures;
        return false;
    }

    const ManualLightingRecord record =
        encodeManualLightingRecord(
            manualLightingState_);

    const std::size_t written =
        preferences_.putBytes(
            kManualLightingKey,
            &record,
            sizeof(record));

    if (written != sizeof(record)) {
        manualLightingStatePersisted_ = false;
        ++stats_.writeFailures;
        return false;
    }

    manualLightingStatePersisted_ = true;
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
        ledMappingProfile_ = profile;
        ledMappingProfileCustomized_ = true;
        ledMappingProfilePersisted_ = false;
        ++stats_.writeFailures;
        return false;
    }

    const bool alreadyCommitted =
        ledMappingProfilePersisted_ &&
        preferences_.getUShort(
            kLedMappingVersionKey,
            0) ==
            LedMappingProfile::kSchemaVersion;

    // For an existing current-schema record, the version marker is already a
    // valid commit marker. Update only the blob so a rejected write preserves
    // the previous durable topology instead of deleting it.
    const std::size_t profileBytes =
        preferences_.putBytes(
            kLedMappingProfileKey,
            &profile,
            sizeof(profile));

    if (profileBytes != sizeof(profile)) {
        ++stats_.writeFailures;
        return false;
    }

    if (!alreadyCommitted) {
        const std::size_t versionBytes =
            preferences_.putUShort(
                kLedMappingVersionKey,
                LedMappingProfile::kSchemaVersion);

        if (versionBytes != sizeof(std::uint16_t)) {
            ++stats_.writeFailures;

            // No valid current-schema marker existed before this transaction.
            // Remove only the newly written blob; unknown/future version state
            // remains untouched for downgrade/recovery.
            preferences_.remove(
                kLedMappingProfileKey);
            return false;
        }
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

    if (!persistenceAvailable_) {
        // Preserve runtime-only commissioning when NVS itself is unavailable.
        ledPixelMaskProfile_ = profile;
        ledPixelMaskProfileCustomized_ = true;
        ledPixelMaskProfilePersisted_ = false;
        ++stats_.writeFailures;
        return false;
    }

    const bool alreadyCommitted =
        ledPixelMaskProfilePersisted_ &&
        preferences_.getUShort(
            kLedPixelMaskVersionKey,
            0) ==
            LedPixelMaskProfile::kSchemaVersion;

    // With an already committed schema marker the blob is the only mutable
    // part. Avoid rewriting the marker: a redundant marker failure after a
    // successful blob write would make rollback semantics ambiguous.
    const std::size_t profileBytes =
        preferences_.putBytes(
            kLedPixelMaskProfileKey,
            &profile,
            sizeof(profile));

    if (profileBytes != sizeof(profile)) {
        ++stats_.writeFailures;
        return false;
    }

    if (!alreadyCommitted) {
        const std::size_t versionBytes =
            preferences_.putUShort(
                kLedPixelMaskVersionKey,
                LedPixelMaskProfile::kSchemaVersion);

        if (versionBytes != sizeof(std::uint16_t)) {
            ++stats_.writeFailures;

            // No valid current-version commit marker existed before this
            // transaction. Remove the newly written blob so boot cannot later
            // pair it with a partially-created marker.
            preferences_.remove(
                kLedPixelMaskProfileKey);
            return false;
        }
    }

    ledPixelMaskProfile_ = profile;
    ledPixelMaskProfileCustomized_ = true;
    ledPixelMaskProfilePersisted_ = true;
    ++stats_.writes;
    return true;
}

bool RuntimeSettings::adoptLedPixelMaskProfileRuntime(
    const LedPixelMaskProfile& profile) {

    if (!profile.validFor(
            ledMappingProfile_)) {

        return false;
    }

    ledPixelMaskProfile_ = profile;
    ledPixelMaskProfileCustomized_ = true;
    ledPixelMaskProfilePersisted_ = false;
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

    ddpBrightness_ =
        config::kDefaultOutputBrightness;

    lightingBrightness_ =
        config::kDefaultOutputBrightness;

    outputEnabled_ = true;
    outputStatePersisted_ = false;

    manualLightingState_ = {};
    manualLightingStatePersisted_ = false;

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
