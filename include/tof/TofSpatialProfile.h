#pragma once

#include <cstdint>
#include <type_traits>

#include "core/ScreenGeometry.h"
#include "tof/TofPlaneChangeGate.h"
#include "tof/TofTypes.h"

namespace ambilight {

struct TofSpatialProfile {
    static constexpr std::uint16_t kSchemaVersion = 1;

    // Fixed-point tenths of a millimetre keep persisted geometry deterministic
    // while preserving the current 1437.5 mm perimeter width exactly.
    std::uint16_t widthMmX10 = 14375;
    std::uint16_t heightMmX10 = 10000;

    std::int16_t sensorOffsetXmmX10 = 0;
    std::int16_t sensorOffsetYmmX10 = 0;
    std::int16_t ledPlaneZmmX10 = 0;

    std::uint16_t planeDeadbandMmX10 = 100;

    std::uint8_t rotationQuarterTurns = 0;
    std::uint8_t mirrorX = 0;

    bool valid() const {
        // 100..5000 mm covers normal display geometries without accepting
        // corrupted zero/tiny values or absurd persisted blobs.
        if (widthMmX10 < 1000 ||
            widthMmX10 > 50000 ||
            heightMmX10 < 1000 ||
            heightMmX10 > 50000) {

            return false;
        }

        if (planeDeadbandMmX10 < 10 ||
            planeDeadbandMmX10 > 5000) {

            return false;
        }

        if (rotationQuarterTurns > 3 ||
            mirrorX > 1) {

            return false;
        }

        // Sensor offsets beyond 2 m or LED plane offsets beyond 1 m are not
        // credible for this installation and usually indicate corrupted NVS.
        if (sensorOffsetXmmX10 < -20000 ||
            sensorOffsetXmmX10 > 20000 ||
            sensorOffsetYmmX10 < -20000 ||
            sensorOffsetYmmX10 > 20000 ||
            ledPlaneZmmX10 < -10000 ||
            ledPlaneZmmX10 > 10000) {

            return false;
        }

        return true;
    }

    float widthMm() const {
        return
            static_cast<float>(
                widthMmX10) /
            10.0F;
    }

    float heightMm() const {
        return
            static_cast<float>(
                heightMmX10) /
            10.0F;
    }

    float sensorOffsetXmm() const {
        return
            static_cast<float>(
                sensorOffsetXmmX10) /
            10.0F;
    }

    float sensorOffsetYmm() const {
        return
            static_cast<float>(
                sensorOffsetYmmX10) /
            10.0F;
    }

    float ledPlaneZmm() const {
        return
            static_cast<float>(
                ledPlaneZmmX10) /
            10.0F;
    }

    float planeDeadbandMm() const {
        return
            static_cast<float>(
                planeDeadbandMmX10) /
            10.0F;
    }

    TofGridTransform transform() const {
        TofGridTransform result;

        switch (rotationQuarterTurns) {
        case 1:
            result.rotation =
                TofRotation::Deg90;
            break;
        case 2:
            result.rotation =
                TofRotation::Deg180;
            break;
        case 3:
            result.rotation =
                TofRotation::Deg270;
            break;
        default:
            result.rotation =
                TofRotation::Deg0;
            break;
        }

        result.mirrorX =
            mirrorX != 0;

        return result;
    }

    PerimeterScreenGeometry perimeterGeometry() const {
        const float halfWidth =
            widthMm() / 2.0F;

        const float halfHeight =
            heightMm() / 2.0F;

        const float offsetX =
            sensorOffsetXmm();

        const float offsetY =
            sensorOffsetYmm();

        const float z =
            ledPlaneZmm();

        const auto point =
            [offsetX, offsetY, z](
                float screenX,
                float screenY) {

                return ScreenPointMm{
                    screenX - offsetX,
                    screenY - offsetY,
                    z
                };
            };

        return {{
            {
                SegmentId::Top,
                point(-halfWidth, +halfHeight),
                point(+halfWidth, +halfHeight)
            },
            {
                SegmentId::Right,
                point(+halfWidth, +halfHeight),
                point(+halfWidth, -halfHeight)
            },
            {
                SegmentId::Bottom,
                point(+halfWidth, -halfHeight),
                point(-halfWidth, -halfHeight)
            },
            {
                SegmentId::Left,
                point(-halfWidth, -halfHeight),
                point(-halfWidth, +halfHeight)
            }
        }};
    }

    TofPlaneChangeGateConfig planeGateConfig() const {
        TofPlaneChangeGateConfig config;
        config.geometry =
            perimeterGeometry();

        config.wallDeltaDeadbandMm =
            planeDeadbandMm();

        return config;
    }
};

static_assert(
    std::is_trivially_copyable<TofSpatialProfile>::value,
    "Persisted spatial profile must remain trivially copyable");

static_assert(
    sizeof(TofSpatialProfile) <= 32,
    "Persisted spatial profile unexpectedly grew");

} // namespace ambilight
