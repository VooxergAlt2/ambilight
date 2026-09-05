# ESP32-C6 Ambilight Controller

Custom ESP32-C6 Ambilight endpoint for HyperHDR.

## Current stage

Stage 15 projects the 2D VL53L5CX wall plane onto the full LED perimeter. Every segment now has independent logical start/end shadow gains, so yaw and pitch both create correction inside segments. Physical ToF gain application is still disabled.

Physical LEDs still receive original HyperHDR RGB.

## Why this matters

ToF is environmental state, not video state.

Even if the RGB image is static, moving the TV should eventually be able to update brightness compensation.

The firmware now caches the latest RGB frame and can shadow-rerender it when gain state changes.

## Render triggers

- new RGB frame
- changing gain profile
- both together

Gain-only rerenders are limited to about 60 Hz.

## Gain target polling

The main loop polls the small GainSnapshot at up to 100 Hz.

If one mutex copy fails, the last successful snapshot is retained.

Renderer freshness rules decide when it is truly stale.

## DDP latency metrics stay clean

Only genuinely new DDP RGB frames update the frame-age histogram.

Gain-only rerenders of an old cached frame do not contaminate network p50/p95/p99.

## Debug commands

    t
raw ToF map

    g
processed LEFT/CENTER/RIGHT geometry

    p
robust 2D wall plane, yaw/pitch and residual quality

    k
ToF gain snapshot

    c
5-second calibration capture

    r
renderer shadow + target/effective + scheduler stats

    x
10-second aggressive shadow probe

## Expected scheduler behavior during x

With an RGB frame already cached:

- gain-only renders rise while endpoints slew
- deferrals may rise between 60 Hz opportunities
- once effective profile reaches target, gain-only activity stops
- when x expires, gain-only renders resume while returning to real target
- physical LEDs remain visually unchanged

## USB/AWA

Preserved separately in:

    stage/07-usb-awa

Active development remains Wi-Fi/DDP.
