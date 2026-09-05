#include "tof/TofPlaneEstimator.h"

#include <algorithm>
#include <cmath>

#include "tof/TofGrid.h"

namespace ambilight {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kPivotEpsilon = 1e-9;

double degToRad(double degrees) {
    return degrees * kPi / 180.0;
}

std::int16_t toCentiDegrees(double radians) {
    const double degrees =
        radians * 180.0 / kPi;

    const double scaled =
        std::round(degrees * 100.0);

    if (scaled <= -32768.0) {
        return -32768;
    }

    if (scaled >= 32767.0) {
        return 32767;
    }

    return static_cast<std::int16_t>(scaled);
}

std::uint16_t clampResidualMm(double value) {
    if (value <= 0.0) {
        return 0;
    }

    if (value >= 65535.0) {
        return 65535;
    }

    return static_cast<std::uint16_t>(
        std::round(value));
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

        if (std::abs(matrix[best][pivot]) <
            kPivotEpsilon) {
            return false;
        }

        if (best != pivot) {
            for (std::size_t column = pivot;
                 column < 4;
                 ++column) {

                std::swap(
                    matrix[pivot][column],
                    matrix[best][column]);
            }
        }

        const double divisor =
            matrix[pivot][pivot];

        for (std::size_t column = pivot;
             column < 4;
             ++column) {

            matrix[pivot][column] /=
                divisor;
        }

        for (std::size_t row = 0; row < 3; ++row) {
            if (row == pivot) {
                continue;
            }

            const double factor =
                matrix[row][pivot];

            for (std::size_t column = pivot;
                 column < 4;
                 ++column) {

                matrix[row][column] -=
                    factor *
                    matrix[pivot][column];
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

    for (std::size_t index = 0;
         index < points.size();
         ++index) {

        if (!accepted[index]) {
            continue;
        }

        const Point& point =
            points[index];

        const double w =
            point.weight;

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

    std::sort(
        values.begin(),
        values.begin() + count);

    if ((count & 1U) != 0U) {
        return values[count / 2];
    }

    return (
        values[count / 2 - 1] +
        values[count / 2]) /
        2.0;
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
    double tanHalfAxisFov,
    Point& point) const {

    const std::size_t rawIndex =
        tofRawIndexForNormalized(
            normalizedRow,
            normalizedCol,
            transform);

    const std::int16_t rawDistance =
        raw.distanceMm[rawIndex];

    const std::uint8_t status =
        raw.targetStatus[rawIndex];

    if (rawDistance <= 0) {
        return false;
    }

    const auto distanceMm =
        static_cast<std::uint16_t>(
            rawDistance);

    if (distanceMm < config_.minDistanceMm ||
        distanceMm > config_.maxDistanceMm) {
        return false;
    }

    double weight = 0.0;

    if (status == 5) {
        weight = 1.0;
    } else if (status == 9) {
        weight =
            std::max(
                0.0,
                static_cast<double>(
                    config_.status9Weight));
    } else {
        return false;
    }

    if (weight <= 0.0) {
        return false;
    }

    // The normalized grid is defined in screen-like coordinates:
    // column increases to the right, row increases downward.
    //
    // Zone centers occupy +/-0.875 of the square FoV half-span.
    const double normalizedX =
        (
            static_cast<double>(
                normalizedCol) -
            3.5
        ) / 4.0;

    const double normalizedY =
        (
            3.5 -
            static_cast<double>(
                normalizedRow)
        ) / 4.0;

    const double rayX =
        normalizedX *
        tanHalfAxisFov;

    const double rayY =
        normalizedY *
        tanHalfAxisFov;

    constexpr double rayZ = 1.0;

    const double rayNorm =
        std::sqrt(
            rayX * rayX +
            rayY * rayY +
            rayZ * rayZ);

    const double scale =
        static_cast<double>(
            distanceMm) /
        rayNorm;

    point.xMm =
        rayX * scale;

    point.yMm =
        rayY * scale;

    point.zMm =
        rayZ * scale;

    point.weight =
        weight;

    return true;
}

TofPlaneEstimate TofPlaneEstimator::estimate(
    const TofRawFrame& raw,
    const TofGridTransform& transform) const {

    TofPlaneEstimate result;

    if (config_.diagonalFovDeg <= 1.0F ||
        config_.diagonalFovDeg >= 170.0F) {
        return result;
    }

    const double diagonalHalfAngle =
        degToRad(
            static_cast<double>(
                config_.diagonalFovDeg) /
            2.0);

    // Square FoV:
    // tan(diagonalHalf) = sqrt(2) * tan(axisHalf)
    const double tanHalfAxisFov =
        std::tan(diagonalHalfAngle) /
        std::sqrt(2.0);

    std::array<Point, kTofZoneCount> points{};
    std::array<bool, kTofZoneCount> accepted{};

    std::size_t candidateCount = 0;

    for (std::size_t row = 0;
         row < kTofGridHeight;
         ++row) {

        for (std::size_t col = 0;
             col < kTofGridWidth;
             ++col) {

            Point point;

            if (!makePoint(
                    raw,
                    row,
                    col,
                    transform,
                    tanHalfAxisFov,
                    point)) {
                continue;
            }

            points[candidateCount] =
                point;

            accepted[candidateCount] =
                true;

            ++candidateCount;
        }
    }

    result.candidates =
        static_cast<std::uint8_t>(
            candidateCount);

    if (candidateCount <
        config_.minAcceptedZones) {
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

    std::array<double, kTofZoneCount>
        residuals{};

    std::array<double, kTofZoneCount>
        signedResiduals{};

    for (std::size_t index = 0;
         index < candidateCount;
         ++index) {

        signedResiduals[index] =
            orthogonalResidualMm(
                points[index],
                initial);
    }

    const double residualMedian =
        median(
            signedResiduals,
            candidateCount);

    for (std::size_t index = 0;
         index < candidateCount;
         ++index) {

        residuals[index] =
            std::abs(
                signedResiduals[index] -
                residualMedian);
    }

    const double residualMad =
        median(
            residuals,
            candidateCount);

    const double residualWindow =
        std::max(
            static_cast<double>(
                config_.minResidualWindowMm),
            residualMad *
                static_cast<double>(
                    config_.residualMadMultiplier));

    std::size_t acceptedCount = 0;

    for (std::size_t index = 0;
         index < candidateCount;
         ++index) {

        const double centeredResidual =
            std::abs(
                signedResiduals[index] -
                residualMedian);

        accepted[index] =
            centeredResidual <=
            residualWindow;

        if (accepted[index]) {
            ++acceptedCount;
        }
    }

    result.accepted =
        static_cast<std::uint8_t>(
            acceptedCount);

    result.residualMedianMm =
        clampResidualMm(
            std::abs(
                residualMedian));

    result.residualMadMm =
        clampResidualMm(
            residualMad);

    if (acceptedCount <
        config_.minAcceptedZones) {
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
            static_cast<double>(
                config_.minDistanceMm) ||
        refined.interceptMm >
            static_cast<double>(
                config_.maxDistanceMm)) {
        return result;
    }

    result.interceptMm =
        static_cast<float>(
            refined.interceptMm);

    result.slopeX =
        static_cast<float>(
            refined.slopeX);

    result.slopeY =
        static_cast<float>(
            refined.slopeY);

    // Positive slopeX means the wall is farther away toward normalized right.
    // Positive slopeY means the wall is farther away toward normalized top.
    result.yawCentiDeg =
        toCentiDegrees(
            std::atan(
                refined.slopeX));

    result.pitchCentiDeg =
        toCentiDegrees(
            std::atan(
                refined.slopeY));

    result.valid = true;
    return result;
}

} // namespace ambilight
