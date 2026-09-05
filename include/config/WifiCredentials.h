#pragma once

#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef AMBILIGHT_WIFI_SSID
#define AMBILIGHT_WIFI_SSID ""
#endif

#ifndef AMBILIGHT_WIFI_PASSWORD
#define AMBILIGHT_WIFI_PASSWORD ""
#endif

namespace ambilight::config {

constexpr const char* kWifiSsid = AMBILIGHT_WIFI_SSID;
constexpr const char* kWifiPassword = AMBILIGHT_WIFI_PASSWORD;

constexpr bool wifiCredentialsPresent() {
    return kWifiSsid[0] != '\0';
}

} // namespace ambilight::config
