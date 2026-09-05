# ESP32-C6 Ambilight Controller

Custom ESP32-C6 Ambilight endpoint for HyperHDR.

## Current stage

Stage 12 keeps the active transport at one PC over Wi-Fi/DDP and models the complete future ToF-to-render timing path in shadow mode.

Physical LEDs still receive original HyperHDR RGB.

## Current shadow pipeline

    VL53L5CX ~10 Hz
      -> geometry
      -> gain model
      -> target RenderGainContext
      -> RenderGainController ~60 Hz
      -> effective RenderGainContext
      -> LedRenderer shadow RGB
      -> ShadowRenderPolicy
      -> ORIGINAL RGB
      -> PARLIO x4

## Why the controller exists

ToF updates much slower than video.

Future gains therefore need render-rate interpolation instead of 10 Hz brightness steps.

Current shadow slew:

    8192 Q12 / second

with unity:

    4096 = 100%

Valid changes are smoothed.

Fail-open immediately returns to 100%.

## Debug commands

    t

Raw 8x8 ToF.

    g

Processed LEFT/CENTER/RIGHT geometry.

    k

ToF gain snapshot.

    c

Five-second calibration capture.

    r

Renderer target/effective shadow state and timing.

    x

Ten-second aggressive shadow self-test:

- TOP 100% -> 75%
- RIGHT 75%
- BOTTOM 50% -> 100%
- LEFT 25%

Even during x, physical LEDs remain original RGB.

## Current calibration

Still identity:

    50 mm   -> 100%
    4000 mm -> 100%

So x is the easiest way to exercise non-unity shadow behavior before real calibration exists.

## Hardware checks for this stage

Run normal HyperHDR at 60 FPS and compare:

- DDP p50/p95/p99
- render prepare time
- PARLIO show time
- backlog skips

Then run:

    x

and inspect:

    r

Expected:

- target becomes strongly non-unity
- effective gains move gradually
- would-change pixels rise on non-black content
- changed-by-segment counters rise
- physical LEDs do not visibly change
- after probe, effective shadow returns toward real ToF target

## USB/AWA

Preserved separately in:

    stage/07-usb-awa

Active development remains Wi-Fi/DDP.
