# ESP32-C6 Ambilight Controller

Custom ESP32-C6 Ambilight endpoint for HyperHDR.

## Current development stage

The repository is currently at Stage 1: prove four synchronized WS2812-class outputs through ESP32-C6 PARLIO before adding any realtime transport.

Target physical layout:

- TOP: 230 LEDs
- RIGHT: 160 LEDs
- BOTTOM: 230 LEDs
- LEFT: 160 LEDs
- total logical LEDs: 780

The current firmware runs low-brightness hardware test patterns and a 60 FPS PARLIO timing probe.

## Toolchain

Pinned baseline:

- pioarduino platform 55.03.311
- Arduino-ESP32 3.3.11
- ESP-IDF 5.5.5 underneath Arduino
- LiteLED 3.2.0

The official PlatformIO espressif32 platform still ships an older Arduino core, so this project intentionally uses the pioarduino platform release.

## Build

    pio run

Upload and monitor with PlatformIO after confirming the exact ESP32-C6 board and the provisional GPIO mapping in include/config/BoardConfig.h.

## Stage 1 safety

Test brightness is intentionally limited to 32/255. LED power distribution, fusing, level shifting, and final GPIO selection are hardware validation items and are not implied by a successful firmware build.

## Next milestones

1. Validate four-lane PARLIO on hardware.
2. Introduce a transport-independent RGB frame core.
3. Add Wi-Fi.
4. Add HyperHDR DDP receive/reassembly for one PC.
5. Stress-test the working Wi-Fi Ambilight.
6. Add USB/AWA independently.
7. Add VL53L5CX and adaptive per-side brightness.
8. Add USB-primary / Wi-Fi-fallback only after both transports are proven separately.

See docs/architecture.md for the current boundaries.
