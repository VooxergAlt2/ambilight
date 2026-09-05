#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "tof/TofTypes.h"

namespace ambilight {

struct TofPlaneEstimatorConfig {
    std::uint16_t minDistanceMm = 50;
    std::uint16_t maxDistanceMm = 4000;

    std::uint8_t minAcceptedZones = 16;

    // Second-pass robust residual gate:
    // |orthogonal residual| <= max(minResidualWindowMm, MAD * multiplier)
    std::uint16_t minResidualWindowMm = 40;
    std::uint8_t residualMadMultiplier = 4;

    // Status 5 is full-confidence. ST also documents statuses 6 and 9 as
    // usable ranges; keep them at lower weight for plane fitting.
    float status6Weight = 0.5F;
    float status9Weight = 0.5F;
};

class TofPlaneEstimator {
public:
    explicit TofPlaneEstimator(
        TofPlaneEstimatorConfig config = {})
        : config_(config) {}

    TofPlaneEstimate estimate(
        const TofRawFrame& raw,
        const TofGridTransform& transform) const;

private:
    struct Point {
        double xMm = 0.0;
        double yMm = 0.0;
        double zMm = 0.0;
        double weight = 0.0;
    };

    struct Plane {
        double slopeX = 0.0;
        double slopeY = 0.0;
        double interceptMm = 0.0;
        bool valid = false;
    };

    static bool solve3x3(
        double matrix[3][4],
        double solution[3]);

    static bool fitWeighted(
        const std::array<Point, kTofZoneCount>& points,
        const std::array<bool, kTofZoneCount>& accepted,
        std::size_t count,
        Plane& plane);

    static double median(
        std::array<double, kTofZoneCount> values,
        std::size_t count);

    static double orthogonalResidualMm(
        const Point& point,
        const Plane& plane);

    bool makePoint(
        const TofRawFrame& raw,
        std::size_t normalizedRow,
        std::size_t normalizedCol,
        const TofGridTransform& transform,
        Point& point) const;

    TofPlaneEstimatorConfig config_{};
};

} // namespace ambilight
