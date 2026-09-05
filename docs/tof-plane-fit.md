# Stage 19: robust 2D wall plane correctness

## Purpose

The wall plane is the sole spatial source used to determine the position of the wall behind each LED.

Physical RGB correction remains disabled.

## Sensor distance semantics

VL53L5CX already performs radial-to-perpendicular conversion.

For a flat wall perpendicular to the sensor, all valid zones should report approximately the same `distance_mm`.

Therefore:

    Z = distance_mm

The firmware does not normalize `distance_mm` as a radial ray length.

X/Y are reconstructed with ST's published 8x8 zone-center pitch/yaw table.

## Plane model

    z_mm = intercept_mm + slope_x*x_mm + slope_y*y_mm

Coordinate system:

- +X: screen right
- +Y: screen up
- +Z: from TV/sensor toward wall

Interpretation:

- slope_x: horizontal wall gradient / TV yaw
- slope_y: vertical wall gradient / TV pitch

## Valid range statuses

- status 5: weight 1.0
- status 6: weight 0.5
- status 9: weight 0.5

Other statuses are excluded from the plane fit.

## Robust fit

1. weighted least-squares plane
2. orthogonal residuals
3. residual median and MAD
4. reject points outside:

       max(40 mm, 4 * MAD)

5. refit accepted points
6. recompute residual quality against the final refined plane

Minimum accepted zones:

    16

## Sensor-contract regression

The key native test feeds a flat wall as:

    distance_mm[0..63] = 800

and requires:

    intercept ~= 800
    slope_x ~= 0
    slope_y ~= 0
    yaw ~= 0
    pitch ~= 0

This test prevents reintroducing the old radial-distance assumption.

## Sampling

Normal runtime keeps the VL53L5CX initialized at 1 Hz but transfers/processes a pose frame about once every 12 seconds.

This is intentional: wall/TV geometry changes slowly and does not need video-rate sensing.
