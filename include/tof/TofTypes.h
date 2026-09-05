#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ambilight {

constexpr std::size_t kTofGridWidth = 8;
constexpr std::size_t kTofGridHeight = 8;
constexpr std::size_t kTofZoneCount =
    kTofGridWidth * kTofGridHeight;

enum class TofRotation : std::uint8_t {
    Deg0 = 0,
    Deg90,
    Deg180,
    Deg270
};

struct TofGridTransform {
    TofRotation rotation = TofRotation::Deg0;
    bool mirrorX = false;
};

struct TofRawFrame {
    std::uint64_t timestampUs = 0;

    std::array<std::int16_t, kTofZoneCount> distanceMm{};
    std::array<std::uint8_t, kTofZoneCount> targetStatus{};
};

struct TofBandEstimate {
    bool valid = false;

    std::uint8_t candidates = 0;
    std::uint8_t accepted = 0;

    std::uint16_t rawMedianMm = 0;
    std::uint16_t madMm = 0;
    std::uint16_t robustMedianMm = 0;

    std::uint16_t filteredMm = 0;
};

struct TofPlaneEstimate {
    bool valid = false;

    std::uint8_t candidates = 0;
    std::uint8_t accepted = 0;

    // z_mm = intercept_mm + slope_x * x_mm + slope_y * y_mm
    //
    // x increases right, y increases up, z points toward the wall.
    float interceptMm = 0.0F;
    float slopeX = 0.0F;
    float slopeY = 0.0F;

    std::uint16_t residualMedianMm = 0;
    std::uint16_t residualMadMm = 0;

    // Diagnostic orientation only.
    //
    // Positive yaw: wall farther toward normalized right.
    // Positive pitch: wall farther toward normalized top.
    std::int16_t yawCentiDeg = 0;
    std::int16_t pitchCentiDeg = 0;
};

struct TofGeometrySnapshot {
    std::uint32_t generation = 0;
    std::uint64_t timestampUs = 0;

    TofBandEstimate left{};
    TofBandEstimate center{};
    TofBandEstimate right{};

    TofPlaneEstimate plane{};

    bool valid = false;
    std::uint8_t acceptedZones = 0;

    // Positive means the normalized right side is farther from the wall.
    std::int16_t rightMinusLeftMm = 0;
};

} // namespace ambilight
