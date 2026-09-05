# ESP32-C6 Ambilight Controller

Custom ESP32-C6 Ambilight endpoint for HyperHDR.

## Current development stage

Stage 4 is a stacked branch that adds a tested DDP frame parser/reassembler without yet opening UDP port 4048.

Implemented layers:

1. four synchronized ESP32-C6 PARLIO outputs
2. fixed logical RGB frame of 780 LEDs
3. task-level FrameMailbox and logical-to-physical mapper
4. Wi-Fi STA with power-save disabled and non-blocking reconnect
5. pure DDP v1 RGB reassembly for the HyperHDR 22 packet pattern

Current runtime still uses generated RGB frames. DDP packets cannot drive LEDs until Stage 5.

## DDP assumptions

The first network protocol implementation intentionally targets one controller layout:

- 780 RGB LEDs
- 2340 RGB payload bytes
- DDP version 1
- RGB type 0x0B
- destination 1
- sequence 1..15

A normal HyperHDR 22 frame is expected in two datagrams: 1440 bytes then 900 bytes.

The assembler supports packet reordering and duplicates and refuses to publish incomplete or conflicting frames.

## Wi-Fi credentials

Copy include/secrets.example.h to include/secrets.h and set:

    AMBILIGHT_WIFI_SSID
    AMBILIGHT_WIFI_PASSWORD

The real secrets file is ignored by Git.

## Build

Firmware:

    pio run -e esp32-c6-devkitc-1

Native tests:

    pio test -e native

## Development order

1. Four-lane PARLIO.
2. Transport-independent frame core.
3. Wi-Fi coexistence and reconnect.
4. DDP parser/reassembly.
5. HyperHDR DDP runtime for one PC.
6. Stress-test working Wi-Fi Ambilight.
7. USB/AWA independently.
8. VL53L5CX and adaptive per-side brightness.
9. USB-primary / Wi-Fi-fallback after both transports are independently proven.
