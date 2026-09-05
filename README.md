# ESP32-C6 Ambilight Controller

Custom ESP32-C6 Ambilight endpoint for HyperHDR.

## Active path

Development is currently Wi-Fi/DDP only.

Stage 8 adds robust VL53L5CX geometry processing while leaving the RGB frame path untouched.

    HyperHDR
        |
        v
    Wi-Fi / DDP
        |
        v
    ESP32-C6
        |
        v
    FrameMailbox -> LedRenderer -> PARLIO x4

In parallel:

    VL53L5CX 8x8 @ 10 Hz
        |
        v
    raw ToF frame
        |
        v
    TofProcessor
        |
        v
    LEFT / CENTER / RIGHT diagnostic geometry

No ToF value modifies brightness yet.

## ToF processing

Normalized bands:

- LEFT: columns 0..2
- CENTER: columns 3..4
- RIGHT: columns 5..7

Filtering:

- statuses 5/9 only
- 50..4000 mm range gate
- median
- MAD-based outlier rejection
- minimum accepted-zone threshold
- 600 ms temporal smoothing
- 10 mm deadband

Orientation supports four rotations and optional horizontal mirroring.

Current provisional config is in `include/config/BoardConfig.h`.

## Debug commands

On debug serial:

    t

prints one raw 8x8 distance/status map.

    g

prints processed geometry:

- candidates / accepted zones
- raw median
- MAD
- robust median
- filtered LEFT/CENTER/RIGHT
- right-minus-left delta

Continuous full-grid logging is intentionally avoided because it would perturb realtime DDP timing.

## Sensor fault handling

VL53L5CX is isolated from Ambilight.

- init happens in a low-priority task
- failed init retries
- five consecutive read failures restart only ToF
- 3 seconds without a successful ToF frame restarts only ToF
- DDP and PARLIO continue running

## USB/AWA

Preserved separately in:

    stage/07-usb-awa

It is not part of the active firmware line yet.

## Build

Firmware:

    pio run -e esp32-c6-devkitc-1

Native tests:

    pio test -e native

## Before adaptive brightness

Capture real `t` + `g` data for parallel/extended/left-yaw/right-yaw TV positions and review whether the three bands behave monotonically and robustly.

Only after that should ToF-derived gains enter LedRenderer.
