# Architecture

## Current stage

Stage 15 uses the fitted 2D wall plane to create spatial shadow gains around the complete LED perimeter.

Physical gains remain disabled.

## Geometry chain

    VL53L5CX 8x8
      -> TofPlaneEstimator
      -> z = c + ax + by
      -> TofPerimeterGainModel
      -> endpoint wall distances
      -> endpoint Q12 gains
      -> PerimeterGainSnapshot
      -> TofRenderGainBridge
      -> RenderGainContext
      -> RenderGainController
      -> LedRenderer per-pixel interpolation
      -> ShadowRenderPolicy
      -> ORIGINAL RGB

## Within-segment correction

Each segment has independent logical start/end values.

Therefore correction is 2D, not just per-side:

- yaw creates horizontal gradients
- pitch creates vertical gradients
- compound orientation creates gradients on all sides

## Parallel legacy path

The older LEFT/CENTER/RIGHT TofGainModel remains active for diagnostics only.

The renderer target now comes from PerimeterGainSnapshot.

## Screen geometry

Screen-space endpoint coordinates are separate from physical LED lane/index mapping.

This is critical:

- physical strip reversal affects wiring only
- screen-space logical direction controls spatial compensation

The current endpoint directions are provisional and must be verified on hardware.

## Current safety

The real distance curve remains unity.

ShadowRenderPolicy still sends original RGB to PARLIO.

## Next concern

Plane projection can extrapolate beyond the area directly observed by the sensor FoV, especially at small wall distance.

A following stage should quantify that extrapolation and endpoint uncertainty before physical gains are allowed.
