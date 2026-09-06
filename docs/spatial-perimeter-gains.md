# Spatial perimeter gains

## Purpose

Project the fitted wall plane behind every logical LED and derive one exact gain value per LED.

Physical RGB correction remains disabled.

## Screen-space geometry

Current provisional LED rectangle:

- width: 1437.5 mm
- height: 1000.0 mm

Current provisional sensor offsets:

- X: 0 mm
- Y: 0 mm
- LED plane Z relative to ToF optical origin: 0 mm

These values require hardware calibration before physical correction is enabled.

## Wall spot for one LED

For each logical LED:

    x_led, y_led, z_led

Wall plane:

    z_wall = intercept + slope_x*x_led + slope_y*y_led

Wall spot:

    x_wall = x_led
    y_wall = y_led
    z_wall = plane(x_led, y_led)

Throw distance:

    distance = z_wall - z_led

The code evaluates this explicitly for all active logical LEDs.

## Why per-LED evaluation

The wall distance is linear along a straight segment, but the empirical distance-to-gain curve may contain multiple piecewise-linear calibration intervals.

Therefore gain is evaluated after distance for each LED:

    gain_i = curve(distance_i)

rather than interpolating only endpoint gains.

## Logical directions

- TOP: left -> right
- RIGHT: top -> bottom
- BOTTOM: right -> left
- LEFT: bottom -> top

These are screen-space directions and are independent of physical strip wiring reversal.

## Fail-open

The full field resolves to unity when:

- wall plane is invalid
- wall plane is stale
- gain curve is invalid
- any LED-wall distance is not finite
- any LED-wall distance lies outside the configured range

No extrapolation-ratio gate is used.
