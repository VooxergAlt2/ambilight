# ESP32-C6 Ambilight Controller

Custom ESP32-C6 Ambilight endpoint for HyperHDR.

## Current development stage

Stage 3 is a stacked branch on top of the transport-independent frame core.

The firmware now proves three independent layers:

1. four synchronized PARLIO outputs
2. one fixed logical RGB frame of 780 LEDs
3. Wi-Fi STA operation with modem power-save disabled

Frame data is still generated locally. DDP has not been added yet.

Current path:

    generated RgbFrame[780]
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

Wi-Fi runs beside this path and must not block it.

## Wi-Fi credentials

Copy:

    include/secrets.example.h

to:

    include/secrets.h

and set:

    AMBILIGHT_WIFI_SSID
    AMBILIGHT_WIFI_PASSWORD

The real secrets file is ignored by Git.

If no credentials are present, firmware still builds and runs with Wi-Fi disabled.

## Toolchain

Pinned baseline:

- pioarduino platform 55.03.311
- Arduino-ESP32 3.3.11
- ESP-IDF 5.5.5 underneath Arduino
- LiteLED 3.2.0

## Build

Firmware:

    pio run -e esp32-c6-devkitc-1

Native mapper tests:

    pio test -e native

## Stage safety

Test brightness is intentionally limited to 32/255. LED power distribution, fusing, level shifting, and final GPIO selection are hardware validation items and are not implied by a successful firmware build.

## Development order

1. Four-lane PARLIO.
2. Transport-independent frame core.
3. Wi-Fi coexistence and reconnect.
4. Pure DDP parser/reassembly.
5. HyperHDR DDP runtime for one PC.
6. Stress-test working Wi-Fi Ambilight.
7. USB/AWA independently.
8. VL53L5CX and adaptive per-side brightness.
9. USB-primary / Wi-Fi-fallback after both transports are independently proven.
