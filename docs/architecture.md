# Architecture

## Current scope

Stage 1 proves the physical LED engine only.

The firmware currently contains no Wi-Fi, DDP, USB/AWA, VL53L5CX, Web UI, OTA, source arbitration, or multi-PC logic.

## Fixed logical frame

HyperHDR-facing geometry will use one logical RGB frame of 780 LEDs:

| Segment | Logical range | LEDs |
| --- | ---: | ---: |
| TOP | 0..229 | 230 |
| RIGHT | 230..389 | 160 |
| BOTTOM | 390..619 | 230 |
| LEFT | 620..779 | 160 |

The logical order is intentionally isolated from the physical GPIO mapping.

## PARLIO layout

LiteLEDpioGroup requires equal lane length. The group therefore uses 230 positions on every lane:

- lane 0: TOP, 230 real LEDs
- lane 1: RIGHT, 160 real LEDs + 70 virtual black positions
- lane 2: BOTTOM, 230 real LEDs
- lane 3: LEFT, 160 real LEDs + 70 virtual black positions

All four lanes are emitted by one ESP32-C6 PARLIO TX unit.

## Design rule for later stages

Incoming transports must publish complete logical RGB frames. They must not call the LED driver directly.

Future data flow:

    DDP or USB/AWA
          |
          v
      RgbFrame[780]
          |
          v
    optional ToF gain
          |
          v
      LedRenderer
          |
          v
     PARLIO x4

This keeps transport parsing, adaptive brightness, and LED timing independently testable.
