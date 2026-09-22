# Exact per-pixel spatial gain field

## Purpose

Every active logical LED may receive its own projected wall distance and gain.
This lets yaw, pitch and combined wall geometry vary continuously inside a side.

For LED `i`:

    P_led_i = (x_i, y_i, z_i)
    z_wall  = intercept + slope_x*x_i + slope_y*y_i
    distance_i = z_wall - z_i
    gain_i = curve(distance_i)

The field is indexed in logical screen order before physical GPIO mapping and
strip reversal.

## Dynamic storage

The old fixed 920-entry gain arrays were removed.

`PerimeterGainSnapshot.logicalGainQ12` and
`RenderGainContext.logicalGainQ12` now allocate exactly for the active topology
on the slow control/ToF path. The realtime renderer does not allocate while
processing video frames.

This has two consequences:

- there is no fixed aggregate gain-field ceiling tied to 920 LEDs;
- static `.bss` usage is lower, while very large topologies consume heap in
  proportion to their active LED count.

If optional gain storage cannot be allocated, the correction path marks the
snapshot unusable and **fails open to unity**. Normal DDP Ambilight rendering is
kept available.

## Why per-LED evaluation

Wall distance is linear along a straight screen segment, but the calibration
curve is piecewise linear in distance. Their composition may cross calibration
knots, so interpolating only between side endpoints can be wrong. Evaluating
each active LED preserves the configured curve exactly.

## Plane deadband and cadence

Small sensor jitter does not rebuild the field. A new fitted wall plane is
compared with the last accepted plane over the screen rectangle.

Current wall-position deadband:

    10 mm

Typical rate domains:

    ToF internal ranging      1 Hz
    pose transfer / fit       ~1 / 12 s
    main target polling       1 Hz
    gain slew / render        up to ~60 Hz

## Validation scope

The software test suite includes gain fields whose logical indices exceed
65535, specifically to prevent aggregate-index narrowing from returning.
Physical LED hardware has been tested up to **230 LEDs on one output**; larger
physical strips have not yet been validated.
