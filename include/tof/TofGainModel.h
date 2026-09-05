#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/GainQ12.h"
#include "tof/TofTypes.h"

namespace ambilight {

struct GainPoint {
    std::uint16_t distanceMm = 0;
    std::uint16_t gainQ12 = kGainUnityQ12;
};

class DistanceGainCurve {
public:
    static constexpr std::size_t kMaxPoints = 8;

    DistanceGainCurve();

    explicit DistanceGainCurve(
        const std::array<GainPoint, kMaxPoints>& points,
        std::size_t count);

    bool configure(
        const std::array<GainPoint, kMaxPoints>& points,
        std::size_t count);

    bool valid() const { return valid_; }
    std::size_t count() const { return count_; }

    std::uint16_t evaluate(std::uint16_t distanceMm) const;

private:
    std::array<GainPoint, kMaxPoints> points_{};
    std::size_t count_ = 0;
    bool valid_ = false;
};

struct TofGainModelConfig {
    DistanceGainCurve curve{};

    std::uint64_t staleTimeoutUs = 30000000;
};

struct GainSnapshot {
    std::uint32_t generation = 0;
    std::uint64_t timestampUs = 0;

    std::uint16_t topQ12 = kGainUnityQ12;
    std::uint16_t bottomQ12 = kGainUnityQ12;
    std::uint16_t leftQ12 = kGainUnityQ12;
    std::uint16_t rightQ12 = kGainUnityQ12;

    bool geometryUsable = false;
    bool failOpen = true;
};

class TofGainModel {
public:
    explicit TofGainModel(
        TofGainModelConfig config = {});

    GainSnapshot evaluate(
        const TofGeometrySnapshot& geometry,
        std::uint64_t nowUs);

    const GainSnapshot& latest() const {
        return latest_;
    }

private:
    static GainSnapshot unitySnapshot(
        std::uint32_t generation,
        std::uint64_t nowUs,
        bool geometryUsable);

    TofGainModelConfig config_{};
    GainSnapshot latest_{};
};

} // namespace ambilight
