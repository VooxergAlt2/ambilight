# Stage 9: ToF gain model

## Purpose

Stage 9 prepares the future distance-to-brightness layer without allowing it to affect LED RGB.

The active frame path is still:

    HyperHDR
      -> Wi-Fi/DDP
      -> FrameMailbox
      -> LedRenderer
      -> PARLIO x4

The diagnostic ToF path is now:

    VL53L5CX
      -> TofProcessor
      -> TofGeometrySnapshot
      -> TofGainModel
      -> GainSnapshot

LedRenderer does not consume GainSnapshot yet.

## Q12 gain format

Unity is:

    4096 = 1.000

Examples:

    3072 = 0.750
    2048 = 0.500

The model intentionally forbids values above unity.

This avoids clipping and color-ratio distortion caused by boosting already-bright RGB channels above 255.

## Distance curve

DistanceGainCurve supports up to eight control points.

Rules:

- at least two points
- strictly increasing distance
- gain <= 1.0
- gain must be non-decreasing with distance

Therefore moving the TV farther from the wall can maintain or increase output, while moving closer can only attenuate.

Between points the model uses integer linear interpolation.

Outside the calibrated range it clamps to the nearest endpoint.

## Shipping calibration

The current firmware deliberately uses:

    50 mm   -> 1.0
    4000 mm -> 1.0

So the model is pass-through.

This is intentional. Real control points must come from physical wall/TV measurements.

## Side mapping

Current diagnostic V1 mapping:

- LEFT gain uses filtered LEFT distance
- RIGHT gain uses filtered RIGHT distance
- TOP gain uses filtered CENTER distance
- BOTTOM gain uses filtered CENTER distance

Top/bottom-from-center is explicitly a temporary V1 assumption.

A later plane-fit model may replace it if real measurements show that pitch or vertical wall geometry matters.

## Fail-open behavior

All gains become unity when:

- geometry is invalid
- any required band is invalid
- geometry timestamp is in the future
- geometry is stale
- gain curve configuration is invalid

Default stale timeout is 1.5 seconds.

If the sensor then remains dead long enough, the independent ToF service also restarts it.

## Diagnostics

Serial command:

    k

prints the current future gain snapshot:

- geometry usable
- fail-open state
- left/right/top/bottom Q12 gain
- percentage equivalent

The output also states that the renderer does not consume the gains yet.

## Native tests

Tests cover:

- default identity curve
- interpolation
- endpoint clamping
- rejection of gain > 1.0
- rejection of non-increasing distance
- rejection of decreasing gain with distance
- four-side mapping
- invalid geometry fail-open
- stale geometry fail-open
- future timestamp fail-open

## Calibration gate

Do not connect GainSnapshot to LedRenderer until real captures exist for:

1. parallel close
2. parallel extended
3. left-close/right-far
4. right-close/left-far
5. intermediate yaw
6. temporary obstruction

The next implementation step after those captures is to derive actual curve control points and verify that brightness response is monotonic and visually stable.
