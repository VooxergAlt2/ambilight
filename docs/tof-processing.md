# ToF processing

## Scope

VL53L5CX is treated as a slow geometry sensor, independent from the realtime RGB path.

Active frame path:

    HyperHDR -> Wi-Fi/DDP -> FrameMailbox -> LedRenderer -> PARLIO x4

Sensor path:

    VL53L5CX 8x8
        -> internal ranging at 1 Hz
        -> one transferred/processed frame about every 12 s
        -> orientation normalization
        -> status/range validation
        -> robust band diagnostics
        -> robust 2D wall plane
        -> plane-change deadband
        -> perimeter/gain rebuild only when geometry materially changes

## Normalized grid

Config controls:

- rotation: 0/90/180/270 degrees
- optional horizontal mirror

These remain compile-time settings until mounting geometry is known.

## Input validity

Usable range statuses:

- 5: full weight
- 6: usable, lower plane-fit weight
- 9: usable, lower plane-fit weight

Distance must be within 50..4000 mm.

## Legacy LEFT/CENTER/RIGHT bands

Normalized columns:

- LEFT: 0..2
- CENTER: 3..4
- RIGHT: 5..7

Each band uses median/MAD outlier rejection. These values remain useful diagnostics but are not the authoritative spatial model for rendering.

## Wall plane

The authoritative spatial model is:

    z = intercept + slope_x*x + slope_y*y

VL53L5CX `distance_mm` is used as perpendicular Z. X/Y are reconstructed from ST's zone-center angle LUT.

Plane fitting uses weighted least squares, median/MAD residual rejection, then a refined fit.

## Plane-change deadband

A valid new plane is compared with the last plane that actually rebuilt the gain field.

The comparison is performed in physical screen space:

1. evaluate both planes at the LED-rectangle corners
2. take maximum absolute wall-Z change
3. if maximum change is below 10 mm:
   - do not rebuild 780 distances
   - do not rebuild 780 gains
   - refresh source generation/timestamp only
4. if change reaches/exceeds 10 mm:
   - accept the new plane
   - rebuild the field

The deadband is cumulative because the comparison reference is not moved on skipped measurements.

Example:

    applied = 600 mm
    candidate = 606 mm -> skip
    candidate = 611 mm -> rebuild

A small slope can still trigger a rebuild if it causes >=10 mm change at a screen edge.

## Freshness

Normal processing interval:

    ~12 s

Gain/plane freshness timeout:

    30 s

A fresh measurement inside the deadband refreshes the existing field's timestamp without recalculating it.

If the plane becomes invalid, the gain path fails open to unity.

## Recovery

- five consecutive ranging read failures trigger restart
- no successful frame for roughly 30 seconds triggers restart
- retry delay: 5 seconds
- RGB/DDP runtime is independent from ToF recovery

## Verification strategy

Software development does not wait for hardware measurements.

Before hardware is available, behavior is locked by:

- sensor-contract fixtures
- synthetic flat/yaw/pitch planes
- outlier tests
- rotation/mirror tests
- plane-deadband tests
- per-LED projection tests
- fail-open tests

Hardware testing later validates mounting orientation, offsets, real noise and photometric calibration values.
