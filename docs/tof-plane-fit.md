# Stage 14: robust 2D wall plane fit

## Purpose

Stage 14 upgrades ToF geometry from coarse LEFT/CENTER/RIGHT bands to a real 2D wall model while preserving the existing band diagnostics.

No physical RGB correction is enabled.

## Plane model

The estimator reconstructs one 3D point for every usable VL53L5CX zone, then fits:

    z_mm = intercept_mm + slope_x * x_mm + slope_y * y_mm

Coordinate system:

- +X: screen right
- +Y: screen up
- +Z: from sensor toward wall

Interpretation:

- slope_x != 0: horizontal angle / yaw
- slope_y != 0: vertical angle / pitch
- both non-zero: compound TV orientation

## Ray reconstruction

VL53L5CX uses a square wide FoV. Stage 14 uses a configurable 65 degree diagonal FoV and reconstructs each 8x8 zone center as a ray.

Zone center rays occupy +/-0.875 of the half-span because the 8 cells sample the square FoV at their centers.

## Robust fit

Input gate:

- status 5: weight 1.0
- status 9: weight 0.5
- distance: 50..4000 mm

Fit:

1. weighted least-squares plane on all candidates
2. calculate orthogonal residuals
3. calculate residual median and MAD
4. reject points beyond:

       max(40 mm, 4 * MAD)

5. refit using accepted points

Minimum accepted zones:

    16

## Why keep LEFT/CENTER/RIGHT

The old band model remains active and independently valid.

This gives two views of the sensor:

- bands: simple robust distances
- plane: geometric wall orientation

On real hardware they can be cross-checked. A plane with poor residuals or suspicious accepted-zone count can be rejected without losing the older diagnostics.

## New diagnostics

Serial command:

    p

prints:

- plane valid
- candidate / accepted zones
- center intercept
- slope X / Y
- yaw / pitch
- residual median
- residual MAD

Periodic STAT also includes compact plane quality and orientation.

## Synthetic tests

Native tests generate exact sensor ranges from a known mathematical plane and verify recovery for:

- flat wall
- simultaneous yaw + pitch
- extreme corner outliers
- status 9 weighting
- insufficient valid zones
- rotation + mirror normalization

## Safety

Stage 14 does not feed plane values into GainSnapshot or LedRenderer.

The physical path is still protected by ShadowRenderPolicy.
