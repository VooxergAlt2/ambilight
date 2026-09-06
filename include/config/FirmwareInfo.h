#pragma once

#include <cstdint>

namespace ambilight::config {

inline constexpr const char kFirmwareName[] =
    "ambilight-c6";

inline constexpr const char kFirmwareVersion[] =
    "0.37.0-dev";

inline constexpr const char kFirmwareTarget[] =
    "ESP32-C6";

inline constexpr std::uint16_t kDevelopmentStage =
    37;

inline constexpr std::uint16_t kSerialProtocolVersion =
    1;

} // namespace ambilight::config
