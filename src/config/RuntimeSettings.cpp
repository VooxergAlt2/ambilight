#include "config/RuntimeSettings.h"

#include "config/BoardConfig.h"

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

} // namespace ambilight
