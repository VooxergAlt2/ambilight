# Architecture

## Current stage

Stage 16 adds confidence diagnostics to the 2D spatial perimeter-gain pipeline.

Physical correction remains disabled.

## Spatial chain

    VL53L5CX
      -> robust 3D wall points
      -> wall plane
      -> observed wall X/Y span
      -> screen perimeter projection
      -> endpoint distances
      -> endpoint gains
      -> RenderGainContext
      -> render-rate slew
      -> per-pixel shadow interpolation
      -> ORIGINAL RGB

## Confidence layers

Plane validity answers:

    did the accepted ToF points support a coherent wall plane?

Projection validity answers:

    can that plane produce sane wall distances at every configured LED endpoint?

Extrapolation warning answers:

    how far outside the directly observed wall patch are we projecting?

These are intentionally separate signals.

## Extrapolation

The estimator records accepted point half-spans X/Y.

The spatial model compares those spans to the configured LED perimeter half-spans.

A ratio over 4.0x currently produces a warning only.

No hard extrapolation fail-open is enabled before real bracket measurements exist.

## Renderer contract

TofRenderGainBridge requires:

- snapshot fresh
- plane usable
- projection usable
- not fail-open

Extrapolation warning alone does not disable shadow gains.

## Remaining mathematical concern

The renderer currently interpolates gain linearly between segment endpoint gains.

Distance itself is linear along a straight segment under the fitted plane, but a calibrated distance-to-gain curve can contain multiple piecewise-linear intervals.

Therefore endpoint-gain interpolation is not exact if one segment spans calibration breakpoints.

The next stage should carry distance through the render profile and evaluate the calibration curve at the actual per-pixel distance.
