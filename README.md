# ESP32-C6 Ambilight Controller

Custom ESP32-C6 Ambilight endpoint for HyperHDR.

## Active firmware

Current development remains **one PC over Wi-Fi/DDP**.

Runtime RGB path:

    HyperHDR
        -> DDP / UDP 4048
        -> ESP32-C6
        -> FrameMailbox
        -> LedRenderer
        -> four PARLIO lanes

ToF runs independently:

    VL53L5CX 8x8
        -> robust geometry
        -> diagnostic gain model

The gain model does not modify RGB yet.

## Current LED geometry

- TOP: 230 LEDs
- RIGHT: 160 LEDs
- BOTTOM: 230 LEDs
- LEFT: 160 LEDs
- total: 780 RGB LEDs

## ToF pipeline

- 8x8 @ 10 Hz
- GPIO6 SDA / GPIO7 SCL, provisional
- status 5/9 only
- median + MAD rejection
- LEFT/CENTER/RIGHT bands
- 600 ms temporal smoothing
- 10 mm deadband
- sensor-only stale/error recovery

## Gain model

Future attenuation is represented in Q12:

    4096 = 100%

Current default calibration is intentionally pass-through:

    50 mm   -> 100%
    4000 mm -> 100%

No guessed attenuation curve is active.

The model already implements:

- monotonic piecewise interpolation
- max gain = 1.0
- invalid geometry fail-open
- stale geometry fail-open
- four future side gains

Current V1 mapping uses CENTER for TOP/BOTTOM.

## Debug commands

    t

raw 8x8 ToF map.

    g

processed LEFT/CENTER/RIGHT geometry.

    k

future gain snapshot and fail-open state.

## USB/AWA

Preserved separately in:

    stage/07-usb-awa

It remains outside the active firmware line.

## Build

Firmware:

    pio run -e esp32-c6-devkitc-1

Native tests:

    pio test -e native

## Current gate

Do not connect gains to LedRenderer until real `t` + `g` captures are collected for the actual TV/bracket/wall geometry.

That data is required to choose a real distance-to-brightness curve instead of inventing one.
