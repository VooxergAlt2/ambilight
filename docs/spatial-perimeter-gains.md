# Stage 15: 2D spatial perimeter gains

## Purpose

Stage 15 projects the robust ToF wall plane onto the LED perimeter and creates a different shadow gain at the logical start and end of every segment.

Physical RGB correction remains disabled.

## Screen-space geometry

Provisional LED rectangle:

- width: 1437.5 mm
- height: 1000.0 mm

These values are derived from the selected strip lengths:

- 230 LEDs at 160 LED/m = 1437.5 mm
- 160 LEDs at 160 LED/m = 1000 mm

Sensor offsets are separately configurable.

Current provisional sensor position:

- X offset: 0 mm
- Y offset: 0 mm
- LED plane Z relative to ToF optical origin: 0 mm

## Logical perimeter directions

Current shadow-only screen-space mapping:

- TOP: left -> right
- RIGHT: top -> bottom
- BOTTOM: right -> left
- LEFT: bottom -> top

This is independent from physical strip reversal.

Hardware patterns must verify these logical directions before physical correction is enabled.

## Plane projection

For every logical segment endpoint:

    wall_z = intercept + slope_x * x + slope_y * y
    distance = wall_z - led_plane_z

Distance is measured along the TV/ToF +Z axis.

The empirical distance-to-gain curve is then evaluated independently at both endpoints.

## Yaw behavior

With pitch approximately zero:

- TOP varies left -> right
- BOTTOM varies right -> left
- RIGHT is approximately uniform
- LEFT is approximately uniform

## Pitch behavior

With yaw approximately zero:

- TOP is approximately uniform
- BOTTOM is approximately uniform
- RIGHT varies top -> bottom
- LEFT varies bottom -> top

This is the required within-segment vertical correction.

## Combined yaw + pitch

When both slopes are non-zero all four segments can have different start/end gains.

The existing LedRenderer interpolates Q12 gain per logical LED between each segment's start and end values.

## Models kept in parallel

TofService now publishes:

1. legacy GainSnapshot from LEFT/CENTER/RIGHT
2. PerimeterGainSnapshot from the 2D plane

The shadow renderer consumes the perimeter model.

Legacy gains remain available for diagnostics and comparison.

## Debug

    k

prints legacy L/C/R-derived gains.

    s

prints spatial endpoint distances and gains for TOP/RIGHT/BOTTOM/LEFT.

    r

prints the target/effective RenderGainContext after bridge and slew.

## Fail-open

Spatial gains fail open to unity when:

- wall plane is invalid
- geometry timestamp is stale/future
- gain curve is invalid
- any predicted endpoint distance is outside 30..4000 mm

Renderer-side freshness and RenderGainController fail-open remain active as additional protection.

## Current curve

The shipping curve is still identity:

    50 mm   -> 100%
    4000 mm -> 100%

Therefore Stage 15 exercises all 2D geometry and gradient plumbing without altering shadow RGB from real ToF.

The x debug probe remains available to exercise non-unity gradients.

## Native tests

Tests explicitly cover:

- flat wall
- yaw-only horizontal gradients
- pitch-only vertical gradients
- combined yaw + pitch on all segments
- LED-plane Z offset
- stale/invalid plane fail-open
- endpoint outside valid distance range

Bridge tests verify that spatial start/end gains survive into RenderGainContext unchanged.
