#include <unity.h>

#include "core/ScreenGeometry.h"
#include "tof/TofPlaneChangeGate.h"

using ambilight::PerimeterScreenGeometry;
using ambilight::SegmentId;
using ambilight::TofPlaneChangeAction;
using ambilight::TofPlaneChangeGate;
using ambilight::TofPlaneChangeGateConfig;
using ambilight::TofPlaneEstimate;

namespace {

PerimeterScreenGeometry makeRectangle(
    float widthMm,
    float heightMm) {

    const float hw = widthMm / 2.0F;
    const float hh = heightMm / 2.0F;

    return {{
        {
            SegmentId::Top,
            {-hw, +hh, 0.0F},
            {+hw, +hh, 0.0F}
        },
        {
            SegmentId::Right,
            {+hw, +hh, 0.0F},
            {+hw, -hh, 0.0F}
        },
        {
            SegmentId::Bottom,
            {+hw, -hh, 0.0F},
            {-hw, -hh, 0.0F}
        },
        {
            SegmentId::Left,
            {-hw, -hh, 0.0F},
            {-hw, +hh, 0.0F}
        }
    }};
}

TofPlaneEstimate plane(
    float interceptMm,
    float slopeX = 0.0F,
    float slopeY = 0.0F) {

    TofPlaneEstimate result;
    result.valid = true;
    result.interceptMm = interceptMm;
    result.slopeX = slopeX;
    result.slopeY = slopeY;
    return result;
}

TofPlaneChangeGate makeGate(
    float deadbandMm = 10.0F) {

    TofPlaneChangeGateConfig config;
    config.geometry =
        makeRectangle(
            1000.0F,
            500.0F);
    config.wallDeltaDeadbandMm =
        deadbandMm;

    return TofPlaneChangeGate(config);
}

} // namespace

void test_first_valid_plane_recalculates() {
    auto gate = makeGate();

    const auto decision =
        gate.observe(
            plane(600.0F));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            TofPlaneChangeAction::Recalculate),
        static_cast<int>(
            decision.action));

    TEST_ASSERT_TRUE(
        gate.hasAcceptedPlane());
}

void test_small_plane_change_refreshes_only() {
    auto gate = makeGate();

    gate.observe(
        plane(600.0F));

    const auto decision =
        gate.observe(
            plane(606.0F));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            TofPlaneChangeAction::RefreshOnly),
        static_cast<int>(
            decision.action));

    TEST_ASSERT_FLOAT_WITHIN(
        0.01F,
        6.0F,
        decision.maxWallDeltaMm);
}

void test_deadband_is_cumulative_from_last_applied_plane() {
    auto gate = makeGate();

    gate.observe(
        plane(600.0F));

    const auto firstSmall =
        gate.observe(
            plane(606.0F));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            TofPlaneChangeAction::RefreshOnly),
        static_cast<int>(
            firstSmall.action));

    // Compared with the last APPLIED 600 mm plane, not the previous 606 mm
    // candidate. Slow real motion therefore cannot disappear forever.
    const auto accumulated =
        gate.observe(
            plane(611.0F));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            TofPlaneChangeAction::Recalculate),
        static_cast<int>(
            accumulated.action));

    TEST_ASSERT_FLOAT_WITHIN(
        0.01F,
        11.0F,
        accumulated.maxWallDeltaMm);
}

void test_small_slope_can_be_material_at_screen_edge() {
    auto gate = makeGate();

    gate.observe(
        plane(600.0F));

    // Half-width is 500 mm. slopeX=0.03 moves opposite screen edges by
    // +/-15 mm, which is material even though the raw slope number is small.
    const auto decision =
        gate.observe(
            plane(
                600.0F,
                0.03F,
                0.0F));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            TofPlaneChangeAction::Recalculate),
        static_cast<int>(
            decision.action));

    TEST_ASSERT_FLOAT_WITHIN(
        0.05F,
        15.0F,
        decision.maxWallDeltaMm);
}


void test_runtime_gate_config_resets_accepted_plane_reference() {
    auto gate = makeGate(
        10.0F);

    gate.observe(
        plane(600.0F));

    TofPlaneChangeGateConfig config;
    config.geometry =
        makeRectangle(
            2000.0F,
            1000.0F);

    config.wallDeltaDeadbandMm =
        20.0F;

    gate.setConfig(
        config);

    TEST_ASSERT_FALSE(
        gate.hasAcceptedPlane());

    // Even a numerically identical plane is a new reference after geometry
    // changes and must force the next full projection rebuild.
    const auto decision =
        gate.observe(
            plane(600.0F));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            TofPlaneChangeAction::Recalculate),
        static_cast<int>(
            decision.action));
}

void test_invalid_plane_fails_open_once_and_resets_reference() {
    auto gate = makeGate();

    gate.observe(
        plane(600.0F));

    TofPlaneEstimate invalid;

    const auto lost =
        gate.observe(invalid);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            TofPlaneChangeAction::FailOpen),
        static_cast<int>(
            lost.action));

    TEST_ASSERT_FALSE(
        gate.hasAcceptedPlane());

    const auto stillLost =
        gate.observe(invalid);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            TofPlaneChangeAction::None),
        static_cast<int>(
            stillLost.action));

    const auto recovered =
        gate.observe(
            plane(602.0F));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            TofPlaneChangeAction::Recalculate),
        static_cast<int>(
            recovered.action));
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_first_valid_plane_recalculates);
    RUN_TEST(test_small_plane_change_refreshes_only);
    RUN_TEST(test_deadband_is_cumulative_from_last_applied_plane);
    RUN_TEST(test_small_slope_can_be_material_at_screen_edge);
    RUN_TEST(test_runtime_gate_config_resets_accepted_plane_reference);
    RUN_TEST(test_invalid_plane_fails_open_once_and_resets_reference);

    return UNITY_END();
}
