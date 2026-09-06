# ToF gain model

## Purpose

Convert plane-derived LED-to-wall distance into an attenuation factor.

The authoritative render source is the exact active per-LED wall-plane model.

The older LEFT/CENTER/RIGHT GainSnapshot remains diagnostic only.

## Q12 format

    4096 = 1.000
    3072 = 0.750
    2048 = 0.500
    0    = 0.000

Gain above unity is rejected.

## Distance curve

DistanceGainCurve supports up to eight control points.

Rules:

- at least 2 points
- strictly increasing distance
- gain <= 4096
- gain non-decreasing with distance

For each logical LED:

    distance_i = wall_z(x_i, y_i) - z_led_i
    gain_i = curve(distance_i)

The curve is evaluated independently for all active LEDs when the wall plane or calibration profile requires a rebuild.

## Runtime calibration

The active curve is no longer limited to compile-time constants.

Serial:

    q<Enter>                                  status
    q50:2048,500:3072,4000:4096<Enter>       set
    qreset<Enter>                             default

Runtime profile is stored in NVS and restored at boot.

Curve edits are refused in ACTIVE mode.

## Curve update fail-safe

A runtime curve change immediately invalidates existing gain snapshots to unity.

The ToF task then installs the new curve and resets the accepted-plane reference.

The next valid pose therefore rebuilds the entire active gain field even if the TV itself did not move.

## Plane deadband interaction

Normal pose updates inside the 10 mm wall-position deadband do not rebuild gains.

A curve change bypasses this optimization exactly once by resetting PlaneChangeGate.

After the first new-curve rebuild, normal cumulative deadband behavior resumes.

## Freshness

Normal pose processing:

    about every 12 s

Gain freshness:

    30 s

A fresh pose inside the deadband refreshes snapshot age without recalculating the field.

## Default curve

The firmware default remains:

    50 mm   -> 4096
    4000 mm -> 4096

It is neutral by design.

Physical calibration later only needs to replace these runtime control points, not modify firmware architecture.
