# Stage 16: spatial projection confidence

## Purpose

Stage 16 quantifies how far the fitted wall plane is extrapolated from the area directly observed by VL53L5CX to the LED perimeter.

The warning is diagnostic only. Physical gain application remains disabled.

## Why this matters

A valid plane fit can still be based on a relatively small patch of wall.

At short TV-to-wall distances the sensor sees a smaller physical wall area, while the 65-inch LED rectangle remains about 1437.5 x 1000 mm.

Projecting the fitted plane to the LED corners may therefore be a several-times extrapolation.

## Observed wall span

After robust residual rejection, TofPlaneEstimator now publishes:

- observedHalfSpanXmm
- observedHalfSpanYmm

These are the maximum absolute X/Y extents of accepted 3D wall points around the ToF optical origin.

## Requested screen span

TofPerimeterGainModel calculates the maximum absolute X/Y endpoint coordinate from configured screen geometry:

- screenHalfSpanXmm
- screenHalfSpanYmm

## Extrapolation ratio

Reported as permille:

    1000 = 1.0x
    2500 = 2.5x
    4000 = 4.0x

For example:

    screen half-width = 500 mm
    observed half-width = 100 mm
    extrapolation X = 5.0x

## Warning policy

Current recommended diagnostic threshold:

    4.0x

If either axis exceeds it:

    extrapolationWarning = true

This does not fail open in Stage 16.

Reason: close-to-wall operation may naturally require substantial extrapolation, and real hardware data is needed before choosing a hard limit.

## Validity separation

The spatial snapshot distinguishes:

- planeUsable: the wall plane itself is valid/fresh
- projectionUsable: the plane can be projected to all configured LED endpoints within the allowed distance range
- extrapolationWarning: projection is mathematically valid but extends far beyond directly observed wall coverage
- failOpen: gains must resolve to unity

This avoids blaming the plane estimator when the actual failure is screen projection.

## Debug

    p

now prints accepted observed wall half-span.

    s

prints:

- plane usable
- projection usable
- fail-open
- min/max predicted LED-wall distance
- observed X/Y half-span
- screen X/Y half-span
- extrapolation X/Y
- warning state
- each segment endpoint distance/gain

Periodic STAT adds compact:

    spex=X/Y spwarn=yes|no

where X/Y are permille.

## Tests

Tests verify:

- plane fit exposes non-zero observed coverage
- 5.0x X / 2.5x Y extrapolation raises warning
- warning does not fail open
- render bridge additionally requires projectionUsable

## Current safety

The real distance-to-gain curve remains identity and ShadowRenderPolicy still outputs original HyperHDR RGB.
