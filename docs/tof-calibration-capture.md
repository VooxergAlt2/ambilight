# Stage 10: calibration capture

## Purpose

Stage 10 adds a repeatable, fixed-memory way to capture one stable physical TV pose before choosing real brightness calibration points.

The renderer is still unchanged.

## Runtime command

Send:

    c

over debug serial.

The controller then records approximately 5 seconds of new ToF geometry generations.

At 10 Hz this should produce roughly 50 samples.

## What is stored

For each valid geometry frame:

- LEFT robust median
- CENTER robust median
- RIGHT robust median
- MAD for each band
- accepted-zone count for each band
- robust RIGHT minus LEFT distance

The collector stores at most 64 samples in fixed arrays.

No malloc/free occurs during capture.

## Why robust median, not filtered distance

Calibration should reflect the physical pose itself, not the temporal history of the previous pose.

Therefore the capture stores each band robustMedianMm, before the 600 ms temporal filter.

The normal live geometry still keeps its filtered values for runtime diagnostics.

## Summary

At the end of 5 seconds the controller prints:

- total unique geometry frames
- valid frames
- overflow count
- LEFT/CENTER/RIGHT p10
- LEFT/CENTER/RIGHT median
- LEFT/CENTER/RIGHT p90
- median MAD per band
- minimum/maximum accepted zones per band
- median RIGHT minus LEFT

This gives both the representative distance and the stability/quality of that pose.

## Recommended capture sequence

For every capture, note the physical pose in the terminal log manually.

Suggested sequence:

1. PARALLEL_CLOSE
2. PARALLEL_MID
3. PARALLEL_EXTENDED
4. LEFT_CLOSE_RIGHT_FAR
5. LEFT_MID_RIGHT_FAR
6. RIGHT_CLOSE_LEFT_FAR
7. RIGHT_MID_LEFT_FAR

If useful, also capture temporary obstruction separately.

## Interpretation

Healthy pose capture should generally show:

- high valid-frame ratio
- narrow p10..p90 span
- modest MAD
- accepted-zone counts well above minimum
- monotonic LEFT/RIGHT movement as the TV yaws

These captures are for deriving real distance-to-gain points later.

## Safety boundary

Capture code cannot alter RGB.

The pipeline remains:

    HyperHDR -> Wi-Fi/DDP -> FrameMailbox -> LedRenderer -> PARLIO

ToF capture only observes the independent geometry snapshot.
