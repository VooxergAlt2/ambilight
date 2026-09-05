# ToF gain model

## Purpose

Convert LED-to-wall distance into an attenuation factor.

The authoritative render-side spatial source is the 2D wall-plane perimeter model. The older LEFT/CENTER/RIGHT GainSnapshot remains available for diagnostics.

## Q12 gain format

Unity:

    4096 = 1.000

Examples:

    3072 = 0.750
    2048 = 0.500

Values above unity are forbidden. The correction attenuates only.

## Distance curve

DistanceGainCurve supports up to eight points.

Rules:

- at least two points
- strictly increasing distance
- gain <= unity
- gain non-decreasing with distance

Each logical LED evaluates the curve at its own plane-derived wall distance.

## Current default calibration

Until real photometric data exists:

    50 mm   -> 1.0
    4000 mm -> 1.0

This makes the shipping/default profile safe and neutral.

The software path is still completed independently of those future measured coefficients.

## Spatial source

For each of 780 logical LEDs:

    distance_i = fitted_wall_z(x_i, y_i) - z_led_i
    gain_i = curve(distance_i)

The complete 780-value field is recalculated only when the accepted wall plane changes materially.

## Plane deadband interaction

If a fresh ToF plane changes predicted wall position by less than the configured 10 mm maximum over the LED rectangle:

- existing gains remain unchanged
- generation/timestamp are refreshed
- no per-LED curve evaluation is performed

Several small movements accumulate relative to the last accepted plane and eventually trigger a rebuild.

## Fail-open

The field resolves to unity when:

- wall plane is invalid
- source becomes stale
- curve configuration is invalid
- a projected LED-wall distance is invalid/out of range

Freshness timeout is 30 seconds, compatible with the normal ~12-second pose sampling interval.

## Development strategy

Real wall measurements are required only to tune the final calibration points and validate mounting geometry.

They do not block implementation of the complete software pipeline.
