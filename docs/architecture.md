# Architecture

## Current stage

Stage 14 adds a robust 2D wall plane estimate to the existing ToF geometry pipeline.

RGB remains one-PC Wi-Fi/DDP and physical gain application remains disabled.

## ToF geometry

    VL53L5CX 8x8
      -> normalized zone rays
      -> weighted 3D points
      -> robust plane fit
      -> TofPlaneEstimate

In parallel, the existing LEFT/CENTER/RIGHT robust-band processing remains unchanged.

TofGeometrySnapshot now contains both:

- left / center / right
- plane

## Wall model

    z = c + ax + by

where:

- a = horizontal wall slope
- b = vertical wall slope
- c = wall distance at the sensor X/Y origin

This is the geometric basis for within-segment compensation.

A horizontal segment can vary with X.
A vertical segment can vary with Y.
With both a and b non-zero, all four perimeter segments can have distinct gradients.

## Current safety boundary

Plane validity is independent from legacy geometry validity.

No Stage 14 code changes GainSnapshot or RenderGainContext.

Physical LEDs remain original HyperHDR RGB.

## Next stage

Project the fitted wall plane onto calibrated TV/perimeter coordinates and derive separate gain endpoints for every segment.

That stage remains shadow-only.
