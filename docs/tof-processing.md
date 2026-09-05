# Stage 8: ToF processing

## Scope

Stage 8 converts raw VL53L5CX 8x8 ranging data into stable diagnostic LEFT/CENTER/RIGHT distance estimates.

It still does **not** modify LED RGB values.

Active frame path remains:

    HyperHDR -> Wi-Fi/DDP -> FrameMailbox -> LedRenderer -> PARLIO x4

Sensor path:

    VL53L5CX 8x8 @ 10 Hz
        -> raw frame
        -> orientation normalization
        -> status/range validation
        -> band extraction
        -> robust spatial filtering
        -> temporal filtering
        -> TofGeometrySnapshot

## Normalized grid

The processor works in a normalized 8x8 coordinate system.

Config controls:

- rotation: 0/90/180/270 degrees
- optional horizontal mirror

Default is currently rotation 0, mirror off.

These are intentionally compile-time settings until actual sensor mounting is verified.

## Bands

Normalized columns:

- LEFT: 0..2, maximum 24 zones
- CENTER: 3..4, maximum 16 zones
- RIGHT: 5..7, maximum 24 zones

The center band is narrower so left/right estimates remain spatially separated.

## Input validity

A sample is considered a candidate only when:

- target status is 5 or 9
- distance > 0
- distance is between 50 and 4000 mm

## Robust spatial filter

For each band:

1. collect candidate distances
2. compute raw median
3. compute MAD, median absolute deviation
4. outlier window = max(100 mm, 4 * MAD)
5. reject samples outside that window
6. require at least:
   - 6 accepted zones for LEFT/RIGHT
   - 4 accepted zones for CENTER
7. compute robust median from accepted values

This deliberately avoids a plain arithmetic mean, which is too sensitive to one zone seeing an object, bracket, cable, or edge.

## Temporal filter

Each band has independent state.

- time constant: 600 ms
- deadband: 10 mm
- alpha is derived from real frame delta-time
- filter uses integer Q16 arithmetic
- first valid result initializes immediately

Changing sensor update frequency therefore does not silently change the intended smoothing time.

## Diagnostic outputs

Each TofBandEstimate exposes:

- valid
- candidates
- accepted
- raw median
- MAD
- robust median
- filtered distance

Geometry also exposes:

    rightMinusLeftMm = filteredRight - filteredLeft

Positive means the normalized right band is farther from the wall.

This is only a **relative geometry indicator**. It is not yet interpreted as a yaw angle.

## Recovery

Sensor health is independent from Ambilight.

After ranging starts:

- five consecutive ranging read failures trigger ToF restart
- no successful ToF frame for 3 seconds triggers ToF restart
- retry delay is 5 seconds
- DDP/LED runtime is never restarted by ToF recovery

## Native tests

The pure C++ processor is tested with synthetic maps for:

- exact rotation index mapping
- flat wall
- independent LEFT/CENTER/RIGHT distances
- single extreme outlier
- invalid target statuses
- out-of-range measurements
- insufficient valid zones
- rotation + mirror normalization
- step response in both directions
- deadband behavior
- status 9 acceptance

## Hardware capture plan before brightness integration

Collect both raw `t` and processed `g` output for at least:

1. TV parallel, close to wall
2. TV parallel, fully extended
3. left edge close / right edge far
4. right edge close / left edge far
5. intermediate yaw
6. a person/object briefly crossing sensor FoV
7. sensor partially occluded if physically possible

For each pose, observe whether LEFT/CENTER/RIGHT move monotonically and whether MAD/accepted counts remain healthy.

Adaptive brightness must not be implemented until these captures are reviewed.
