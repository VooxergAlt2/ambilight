#include "tof/TofPlaneEstimator.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "tof/TofGrid.h"

namespace ambilight {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kPivotEpsilon = 1e-9;

// Zone-center angles copied from ST's VL53L5CX 8x8 point-cloud reference.
// distance_mm is already the perpendicular Z distance; the sensor performs the
// radial-to-perpendicular conversion internally.
constexpr std::array<double, kTofZoneCount> kZonePitchDeg = {{
    62.85, 66.50, 69.40, 71.08, 71.08, 69.40, 66.50, 62.85,
    66.50, 70.81, 75.05, 77.50, 77.50, 75.05, 70.81, 66.50,
    69.40, 75.05, 78.15, 81.76, 81.76, 78.15, 75.05, 69.40,
    71.08, 77.50, 81.76, 86.00, 86.00, 81.76, 77.50, 71.08,
    71.08, 77.50, 81.76, 86.00, 86.00, 81.76, 77.50, 71.08,
    69.40, 75.05, 78.15, 81.76, 81.76, 78.15, 75.05, 69.40,
    66.50, 70.81, 75.05, 77.50, 77.50, 75.05, 70.81, 66.50,
    62.85, 66.50, 69.40, 71.08, 71.08, 69.40, 66.50, 62.85
}};

constexpr std::array<double, kTofZoneCount> kZoneYawDeg = {{
    135.00, 125.40, 113.20, 98.13, 81.87, 66.80, 54.60, 45.00,
    144.60, 135.00, 120.96, 101.31, 78.69, 59.04, 45.00, 35.40,
    156.80, 149.04, 135.00, 108.45, 71.55, 45.00, 30.96, 23.20,
    171.87, 168.69, 161.55, 135.00, 45.00, 18.45, 11.31, 8.13,
    188.13, 191.31, 198.45, 225.00, 315.00, 341.55, 348.69, 351.87,
    203.20, 210.96, 225.00, 251.55, 288.45, 315.00, 329.04, 336.80,
    203.20, 225.00, 239.04, 258.69, 281.31, 300.96, 315.00, 324.60,
    225.00, 234.60, 246.80, 261.87, 278.13, 293.20, 305.40, 315.00
}};

double degToRad(double degrees) {
    return degrees * kPi / 180.0;
}

std::int16_t toCentiDegrees(double radians) {
    const double degrees = radians * 180.0 / kPi;
    const double scaled = std::round(degrees * 100.0);

    if (scaled <= -32768.0) {
        return -32768;
    }

    if (scaled >= 32767.0) {
        return 32767;
    }

    return static_cast<std::int16_t>(scaled);
}

std::uint16_t clampUnsignedMm(double value) {
    if (value <= 0.0) {
        return 0;
    }

    if (value >= 65535.0) {
        return 65535;
    }

    return static_cast<std::uint16_t>(std::round(value));
}

} // namespace

bool TofPlaneEstimator::solve3x3(
    double matrix[3][4],
    double solution[3]) {

    for (std::size_t pivot = 0; pivot < 3; ++pivot) {
        std::size_t best = pivot;

        for (std::size_t row = pivot + 1; row < 3; ++row) {
            if (std::abs(matrix[row][pivot]) >
                std::abs(matrix[best][pivot])) {
                best = row;
            }
        }

        if (std::abs(matrix[best][pivot]) < kPivotEpsilon) {
            return false;
        }

        if (best != pivot) {
            for (std::size_t column = pivot; column < 4; ++column) {
                std::swap(matrix[pivot][column], matrix[best][column]);
            }
        }

        const double divisor = matrix[pivot][pivot];

        for (std::size_t column = pivot; column < 4; ++column) {
            matrix[pivot][column] /= divisor;
        }

        for (std::size_t row = 0; row < 3; ++row) {
            if (row == pivot) {
                continue;
            }

            const double factor = matrix[row][pivot];

            for (std::size_t column = pivot; column < 4; ++column) {
                matrix[row][column] -=
                    factor * matrix[pivot][column];
            }
        }
    }

    solution[0] = matrix[0][3];
    solution[1] = matrix[1][3];
    solution[2] = matrix[2][3];

    return true;
}

bool TofPlaneEstimator::fitWeighted(
    const std::array<Point, kTofZoneCount>& points,
    const std::array<bool, kTofZoneCount>& accepted,
    std::size_t count,
    Plane& plane) {

    if (count < 3) {
        return false;
    }

    double sxx = 0.0;
    double sxy = 0.0;
    double sx = 0.0;
    double syy = 0.0;
    double sy = 0.0;
    double sw = 0.0;
    double sxz = 0.0;
    double syz = 0.0;
    double sz = 0.0;

    for (std::size_t index = 0; index < points.size(); ++index) {
        if (!accepted[index]) {
            continue;
        }

        const Point& point = points[index];
        const double w = point.weight;

        sxx += w * point.xMm * point.xMm;
        sxy += w * point.xMm * point.yMm;
        sx += w * point.xMm;
        syy += w * point.yMm * point.yMm;
        sy += w * point.yMm;
        sw += w;
        sxz += w * point.xMm * point.zMm;
        syz += w * point.yMm * point.zMm;
        sz += w * point.zMm;
    }

    double matrix[3][4] = {
        {sxx, sxy, sx, sxz},
        {sxy, syy, sy, syz},
        {sx,  sy,  sw, sz}
    };

    double solution[3] = {};

    if (!solve3x3(matrix, solution)) {
        return false;
    }

    plane.slopeX = solution[0];
    plane.slopeY = solution[1];
    plane.interceptMm = solution[2];

    plane.valid =
        std::isfinite(plane.slopeX) &&
        std::isfinite(plane.slopeY) &&
        std::isfinite(plane.interceptMm);

    return plane.valid;
}

double TofPlaneEstimator::median(
    std::array<double, kTofZoneCount> values,
    std::size_t count) {

    if (count == 0) {
        return 0.0;
    }

    std::sort(values.begin(), values.begin() + count);

    if ((count & 1U) != 0U) {
        return values[count / 2];
    }

    return (values[count / 2 - 1] + values[count / 2]) / 2.0;
}

double TofPlaneEstimator::orthogonalResidualMm(
    const Point& point,
    const Plane& plane) {

    const double numerator =
        point.zMm -
        (
            plane.interceptMm +
            plane.slopeX * point.xMm +
            plane.slopeY * point.yMm
        );

    const double denominator =
        std::sqrt(
            1.0 +
            plane.slopeX * plane.slopeX +
            plane.slopeY * plane.slopeY);

    return numerator / denominator;
}

bool TofPlaneEstimator::makePoint(
    const TofRawFrame& raw,
    std::size_t normalizedRow,
    std::size_t normalizedCol,
    const TofGridTransform& transform,
    Point& point) const {

    const std::size_t rawIndex =
        tofRawIndexForNormalized(
            normalizedRow,
            normalizedCol,
            transform);

    const std::int16_t rawDistance = raw.distanceMm[rawIndex];
    const std::uint8_t status = raw.targetStatus[rawIndex];

    if (rawDistance <= 0) {
        return false;
    }

    const auto distanceMm =
        static_cast<std::uint16_t>(rawDistance);

    if (distanceMm < config_.minDistanceMm ||
        distanceMm > config_.maxDistanceMm) {
        return false;
    }

    double weight = 0.0;

    if (status == 5) {
        weight = 1.0;
    } else if (status == 6) {
        weight = std::max(
            0.0,
            static_cast<double>(config_.status6Weight));
    } else if (status == 9) {
        weight = std::max(
            0.0,
            static_cast<double>(config_.status9Weight));
    } else {
        return false;
    }

    if (weight <= 0.0) {
        return false;
    }

    const std::size_t normalizedIndex =
        normalizedRow * kTofGridWidth +
        normalizedCol;

    const double pitch =
        degToRad(kZonePitchDeg[normalizedIndex]);

    const double yaw =
        degToRad(kZoneYawDeg[normalizedIndex]);

    const double sinPitch = std::sin(pitch);
    if (std::abs(sinPitch) < 1e-6) {
        return false;
    }

    // ST point-cloud semantics:
    // Z = distance_mm. X/Y are reconstructed only to locate each zone in the
    // wall plane. Do not normalize distance_mm as if it were radial range.
    const double zMm = static_cast<double>(distanceMm);
    const double hyp = zMm / sinPitch;
    const double cosPitch = std::cos(pitch);

    point.xMm = std::cos(yaw) * cosPitch * hyp;
    point.yMm = std::sin(yaw) * cosPitch * hyp;
    point.zMm = zMm;
    point.weight = weight;

    return true;
}

TofPlaneEstimate TofPlaneEstimator::estimate(
    const TofRawFrame& raw,
    const TofGridTransform& transform) const {

    TofPlaneEstimate result;

    std::array<Point, kTofZoneCount> points{};
    std::array<bool, kTofZoneCount> accepted{};

    std::size_t candidateCount = 0;

    for (std::size_t row = 0; row < kTofGridHeight; ++row) {
        for (std::size_t col = 0; col < kTofGridWidth; ++col) {
            Point point;

            if (!makePoint(
                    raw,
                    row,
                    col,
                    transform,
                    point)) {
                continue;
            }

            points[candidateCount] = point;
            accepted[candidateCount] = true;
            ++candidateCount;
        }
    }

    result.candidates =
        static_cast<std::uint8_t>(candidateCount);

    if (candidateCount < config_.minAcceptedZones) {
        return result;
    }

    Plane initial;

    if (!fitWeighted(
            points,
            accepted,
            candidateCount,
            initial)) {
        return result;
    }

    std::array<double, kTofZoneCount> absoluteResiduals{};
    std::array<double, kTofZoneCount> signedResiduals{};

    for (std::size_t index = 0; index < candidateCount; ++index) {
        signedResiduals[index] =
            orthogonalResidualMm(points[index], initial);
    }

    const double initialResidualMedian =
        median(signedResiduals, candidateCount);

    for (std::size_t index = 0; index < candidateCount; ++index) {
        absoluteResiduals[index] =
            std::abs(
                signedResiduals[index] -
                initialResidualMedian);
    }

    const double initialResidualMad =
        median(absoluteResiduals, candidateCount);

    const double residualWindow =
        std::max(
            static_cast<double>(config_.minResidualWindowMm),
            initialResidualMad *
                static_cast<double>(config_.residualMadMultiplier));

    std::size_t acceptedCount = 0;

    for (std::size_t index = 0; index < candidateCount; ++index) {
        const double centeredResidual =
            std::abs(
                signedResiduals[index] -
                initialResidualMedian);

        accepted[index] = centeredResidual <= residualWindow;

        if (accepted[index]) {
            ++acceptedCount;
        }
    }

    result.accepted =
        static_cast<std::uint8_t>(acceptedCount);

    if (acceptedCount < config_.minAcceptedZones) {
        return result;
    }

    Plane refined;

    if (!fitWeighted(
            points,
            accepted,
            acceptedCount,
            refined)) {
        return result;
    }

    if (refined.interceptMm <
            static_cast<double>(config_.minDistanceMm) ||
        refined.interceptMm >
            static_cast<double>(config_.maxDistanceMm)) {
        return result;
    }

    // Quality metrics belong to the final refined plane.
    std::array<double, kTofZoneCount> refinedSigned{};
    std::array<double, kTofZoneCount> refinedAbsolute{};
    std::size_t refinedCount = 0;

    for (std::size_t index = 0; index < candidateCount; ++index) {
        if (!accepted[index]) {
            continue;
        }

        refinedSigned[refinedCount++] =
            orthogonalResidualMm(points[index], refined);
    }

    const double refinedMedian =
        median(refinedSigned, refinedCount);

    for (std::size_t index = 0; index < refinedCount; ++index) {
        refinedAbsolute[index] =
            std::abs(refinedSigned[index] - refinedMedian);
    }

    result.residualMedianMm =
        clampUnsignedMm(std::abs(refinedMedian));

    result.residualMadMm =
        clampUnsignedMm(
            median(refinedAbsolute, refinedCount));

    result.interceptMm =
        static_cast<float>(refined.interceptMm);

    result.slopeX =
        static_cast<float>(refined.slopeX);

    result.slopeY =
        static_cast<float>(refined.slopeY);

    double maxAbsX = 0.0;
    double maxAbsY = 0.0;

    for (std::size_t index = 0; index < candidateCount; ++index) {
        if (!accepted[index]) {
            continue;
        }

        maxAbsX =
            std::max(maxAbsX, std::abs(points[index].xMm));

        maxAbsY =
            std::max(maxAbsY, std::abs(points[index].yMm));
    }

    result.observedHalfSpanXmm =
        clampUnsignedMm(maxAbsX);

    result.observedHalfSpanYmm =
        clampUnsignedMm(maxAbsY);

    result.yawCentiDeg =
        toCentiDegrees(std::atan(refined.slopeX));

    result.pitchCentiDeg =
        toCentiDegrees(std::atan(refined.slopeY));

    result.valid = true;
    return result;
}

} // namespace ambilight
