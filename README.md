# ESP32-C6 Ambilight Controller

Custom ESP32-C6 Ambilight endpoint for HyperHDR.

## Current development stage

Stage 2 is a stacked branch on top of the Stage 1 four-lane PARLIO engine.

The code now uses the same transport-independent path that future HyperHDR input will use:

    RgbFrame[780]
         |
         v
    FrameMailbox
         |
         v
    LedRenderer
         |
         v
    SegmentMapper
         |
         v
    PARLIO x4

No Wi-Fi, DDP, USB/AWA, VL53L5CX, Web UI, OTA, source arbitration, or multi-PC logic is present yet.

Target physical layout:

- TOP: 230 LEDs
- RIGHT: 160 LEDs
- BOTTOM: 230 LEDs
- LEFT: 160 LEDs
- total logical LEDs: 780
- RGB payload per logical frame: 2340 bytes

## Toolchain

Pinned baseline:

- pioarduino platform 55.03.311
- Arduino-ESP32 3.3.11
- ESP-IDF 5.5.5 underneath Arduino
- LiteLED 3.2.0

The official PlatformIO espressif32 platform still ships an older Arduino core, so this project intentionally uses the pinned pioarduino platform release.

## Build

Firmware:

    pio run -e esp32-c6-devkitc-1

Native mapper tests:

    pio test -e native

Upload and monitor with PlatformIO after confirming the exact ESP32-C6 board and the provisional GPIO mapping in include/config/BoardConfig.h.

## Stage safety

Test brightness is intentionally limited to 32/255. LED power distribution, fusing, level shifting, and final GPIO selection are hardware validation items and are not implied by a successful firmware build.

## Development order

1. Validate four-lane PARLIO on hardware.
2. Validate the frame core and segment mapper.
3. Add Wi-Fi while keeping generated test frames as the source.
4. Add HyperHDR DDP receive/reassembly for one PC.
5. Stress-test the working Wi-Fi Ambilight.
6. Add USB/AWA independently.
7. Add VL53L5CX and adaptive per-side brightness.
8. Add USB-primary / Wi-Fi-fallback only after both transports are proven separately.

See docs/architecture.md for current design boundaries.
