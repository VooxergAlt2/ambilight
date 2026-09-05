# Architecture

## Current stage

Stage 8 keeps the one-PC Wi-Fi/DDP renderer as the only active frame transport and adds a pure C++ VL53L5CX geometry processor.

Active RGB path:

    HyperHDR
      -> Wi-Fi / DDP
      -> DdpUdpService
      -> DdpAssembler
      -> FrameMailbox
      -> LedRenderer
      -> SegmentMapper
      -> PARLIO x4

Independent ToF path:

    VL53L5CX 8x8 @ 10 Hz
      -> TofService
      -> TofRawFrame
      -> TofProcessor
      -> TofGeometrySnapshot

The two paths are intentionally not connected yet.

## RGB transport invariant

ToF cannot:

- modify RgbFrame
- call LedRenderer
- call LedEngine
- block DDP receive
- restart Wi-Fi
- restart the MCU

This keeps the proven DDP renderer isolated while ToF math is validated.

## ToF processing contract

TofProcessor is Arduino-free pure C++.

Input:

    TofRawFrame
      timestampUs
      distanceMm[64]
      targetStatus[64]

Output:

    TofGeometrySnapshot
      LEFT estimate
      CENTER estimate
      RIGHT estimate
      accepted zone count
      rightMinusLeftMm

Each band contains:

- candidate count
- accepted count
- raw median
- MAD
- robust median
- temporally filtered distance
- validity

## Spatial filtering

Normalized 8x8 grid:

- LEFT = columns 0..2
- CENTER = columns 3..4
- RIGHT = columns 5..7

Sample gate:

- status 5 or 9
- distance 50..4000 mm

Outlier rejection:

    threshold = max(100 mm, 4 * MAD)

Minimum accepted samples:

- LEFT/RIGHT: 6
- CENTER: 4

## Orientation

Raw ST zone order is normalized before band extraction.

Compile-time transform supports:

- 0 degrees
- 90 degrees
- 180 degrees
- 270 degrees
- optional horizontal mirror

Current values are provisional until actual mounted-sensor captures are reviewed.

## Temporal filtering

Each band has independent state.

- time constant: 600 ms
- deadband: 10 mm
- alpha derived from actual timestamp delta
- Q16 integer arithmetic
- first valid sample initializes immediately

## ToF recovery

Sensor work stays in a low-priority FreeRTOS task.

Recovery conditions:

- init failure -> retry after 5 s
- five consecutive ranging read failures -> sensor-only restart
- 3 s with no successful ranging frame -> sensor-only restart

DDP and PARLIO remain operational throughout.

## Debug

Serial command:

    t

prints one raw 8x8 map.

Serial command:

    g

prints processed LEFT/CENTER/RIGHT diagnostics.

Full maps are manual only so logging does not become a hidden source of DDP jitter.

## Native test boundary

Pure C++ tests cover:

- logical LED mapping
- DDP assembler
- latency histogram
- ToF processor

The ToF processor tests include rotation mapping, flat wall, asymmetric wall, invalid samples, outliers, low confidence, temporal smoothing, deadband, and status handling.

## Next architectural step

Do not connect ToF to LedRenderer yet.

First collect real measurements at several TV poses and define a calibration/model layer that maps stable geometry to desired side brightness gains.

Only after that layer has independent tests should a GainSnapshot be consumed by LedRenderer.
