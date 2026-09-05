# ESP32-C6 Ambilight Controller

Custom ESP32-C6 Ambilight endpoint for HyperHDR.

## Active firmware

Current development remains one PC over Wi-Fi/DDP.

RGB path:

    HyperHDR
      -> DDP / UDP 4048
      -> ESP32-C6
      -> FrameMailbox
      -> LedRenderer
      -> PARLIO x4

ToF path:

    VL53L5CX
      -> robust LEFT/CENTER/RIGHT geometry
      -> diagnostic fail-open gain model
      -> calibration capture tooling

No ToF-derived value modifies RGB yet.

## Debug commands

    t

One raw 8x8 distance/status map.

    g

Processed geometry with candidate/accepted counts, median, MAD, robust median and filtered L/C/R.

    k

Future Q12 side gains and fail-open state.

    c

Collect about 5 seconds of stable-pose geometry and print a calibration summary.

The calibration summary contains p10/median/p90, median MAD and accepted-zone range for L/C/R.

## Current calibration

Production gain curve is intentionally pass-through:

    50 mm   -> 100%
    4000 mm -> 100%

Real attenuation values are blocked on physical calibration captures.

## Capture procedure

Keep the TV still and run `c` once for each meaningful bracket pose.

Recommended minimum:

- parallel close
- parallel mid
- parallel fully extended
- left close / right far
- right close / left far
- intermediate yaw in both directions

Keep a note beside each serial summary describing the pose.

## USB/AWA

Preserved separately in:

    stage/07-usb-awa

It is not part of the active firmware line.

## Build

Firmware:

    pio run -e esp32-c6-devkitc-1

Native tests:

    pio test -e native

## Current gate

The next step is not renderer integration.

First collect real `c` summaries plus representative `t`/`g` dumps. Those measurements are needed to derive an evidence-based distance-to-brightness curve.
