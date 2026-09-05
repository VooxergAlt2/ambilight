# Exact per-pixel spatial gain field

## Purpose

Every logical LED receives its own wall distance and its own calibration-curve evaluation.

Physical correction can therefore represent yaw, pitch and combined wall geometry continuously inside each segment.

## Exact composition

For LED i:

    P_led_i = (x_i, y_i, z_i)

Wall plane:

    z_wall = intercept + slope_x*x + slope_y*y

Screen-normal wall distance:

    distance_i = z_wall(x_i, y_i) - z_i

Gain:

    gain_i = curve(distance_i)

This is evaluated for all 780 logical LEDs whenever a new plane is accepted.

## Why not endpoint gain interpolation

The wall distance is linear along a straight segment.

The gain curve is piecewise linear in distance.

Therefore the composition may cross calibration knots and is not necessarily one straight gain line between segment endpoints.

Per-LED evaluation preserves the curve exactly.

## Data model

    PerimeterGainSnapshot.logicalGainQ12[780]
    RenderGainContext.logicalGainQ12[780]

The array is indexed in logical screen order before physical lane reversal.

## Plane deadband

The 780-value field is not rebuilt for sensor jitter.

The new plane is compared with the last accepted plane over the screen rectangle.

Current threshold:

    10 mm max wall-position change

Below the threshold:

    refresh timestamp only

At/above the threshold:

    recompute 780 wall distances
    recompute 780 gain values

## Rate domains

ToF internal ranging:

    1 Hz

Pose transfer/plane fit:

    ~1 / 12 s

Large gain target polling by main loop:

    1 Hz

Gain slew/render after an accepted target change:

    up to ~60 Hz

## Performance

One field:

    780 * 2 bytes = 1560 bytes

At the slow pose cadence, explicit evaluation of all 780 LED intersections is inexpensive and preferable to hidden geometric approximations.

## Fail-open

Any invalid/stale spatial source resolves the full field to unity immediately.
