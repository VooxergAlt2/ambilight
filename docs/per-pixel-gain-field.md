# Stage 17: exact per-pixel spatial gain field

## Purpose

Stage 17 removes the last approximation inside a segment.

Earlier stages projected the wall plane to each segment endpoint and then linearly interpolated Q12 gain between those endpoint gains.

That is exact only when the whole segment remains inside one linear interval of the distance-to-gain calibration curve.

Stage 17 instead calculates the wall distance for every logical LED position and evaluates the calibration curve at that distance.

Physical RGB application remains disabled.

## Exact composition

For a straight LED segment under a fitted plane:

    distance(t) = d0 + (d1 - d0) * t

Distance is linear along the segment.

The calibration curve is piecewise linear in distance:

    gain = curve(distance)

Therefore the correct result is:

    gain_i = curve(distance_i)

for every LED i.

It is not generally correct to calculate only:

    gain0 = curve(d0)
    gain1 = curve(d1)

and linearly interpolate gain0..gain1 when d0..d1 crosses calibration knots.

## Concrete test

The native test uses a TOP segment whose wall distance spans:

    300 -> 700 mm

and a calibration knot at:

    500 mm -> Q12 2048

At logical offset 114 of 230 LEDs:

    distance = 499 mm
    exact gain = 2045

The old endpoint-gain interpolation would produce roughly:

    2300

Stage 17 explicitly asserts the exact 2045 result.

## Data model

PerimeterGainSnapshot now contains:

    logicalGainQ12[780]

This field is calculated in the ToF task at sensor/update rate.

RenderGainContext also contains:

    logicalGainQ12[780]

It is renderer-neutral and indexed by HyperHDR logical LED index before physical strip mapping.

## Renderer

LedRenderer now performs:

    gain = context.gainForLogicalIndex(logicalIndex)

No spatial interpolation occurs in LedRenderer.

Physical lane reversal happens afterward and cannot change spatial correction direction.

## Dynamics

RenderGainController now slews all 780 logical gains independently.

This matters when calibration knots move through a segment as TV yaw/pitch changes.

A moving piecewise breakpoint therefore remains smooth instead of forcing the renderer back to an endpoint approximation.

## Debug probe

The x shadow probe still generates deterministic gradients.

It now expands those gradients into the same 780-value logical field used by real spatial ToF gains.

## Diagnostics

The s command still prints each segment endpoint and now also prints its logical midpoint gain.

The r command prints target/effective start, midpoint, and end values for each segment.

This makes within-segment behavior directly observable on hardware.

## Performance

The spatial snapshot is now about 1.6 KB larger.

Because VL53L5CX updates at about 10 Hz, main-loop target polling was reduced from 100 Hz to 20 Hz:

    50 ms polling interval

The render controller still runs at RGB/gain render rate and slews the cached field at up to about 60 Hz.

## Fail-open

Fail-open still resolves the complete 780-value field to unity immediately.

No stale per-pixel attenuation can remain after spatial data becomes unusable.

## Physical safety

Unchanged:

    ShadowRenderPolicy::physicalOutput(original, candidate)
        -> original

No real ToF coefficient reaches physical LEDs.
