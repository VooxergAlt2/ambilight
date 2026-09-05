# Architecture

## Current stage

Stage 10 keeps Wi-Fi/DDP as the only active RGB transport and adds fixed-memory calibration capture tooling around the already isolated ToF pipeline.

RGB:

    HyperHDR
      -> Wi-Fi/DDP
      -> DdpUdpService
      -> DdpAssembler
      -> FrameMailbox
      -> LedRenderer
      -> PARLIO x4

ToF:

    VL53L5CX
      -> TofProcessor
      -> TofGeometrySnapshot
      -> TofGainModel
      -> GainSnapshot
      -> TofCalibrationCapture

There is still no connection from GainSnapshot or calibration data to LedRenderer.

## Calibration collector

TofCalibrationCapture is pure C++.

It stores at most 64 unique valid geometry samples.

Each sample contains only compact diagnostic values, not the full 8x8 raw grid.

Stored per band:

- robust median distance
- MAD
- accepted-zone count

Stored global:

- robust RIGHT minus LEFT delta

## Duplicate control

The main loop may run hundreds of times faster than the 10 Hz ToF sensor.

Runtime therefore forwards only a new geometry generation into the capture collector.

The collector itself also protects against duplicate generations.

## Summary statistics

At capture completion:

- p10 / median / p90 distance
- median MAD
- min/max accepted zones
- median right-minus-left

These are deliberately simple, robust metrics that can be reasoned about from terminal logs.

## Memory

Three bands x 64 samples plus metadata remain small fixed arrays.

No heap allocation occurs during capture.

## Safety

Calibration capture can never:

- change RGB
- invoke LedRenderer
- change DDP source state
- restart Wi-Fi
- restart MCU

## Next gate

Use real captures to choose actual gain curve points.

Only then should the renderer begin consuming GainSnapshot.
