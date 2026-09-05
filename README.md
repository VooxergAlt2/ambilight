# ESP32-C6 Ambilight Controller

Custom ESP32-C6 Ambilight endpoint for HyperHDR.

## Current development stage

Stage 6 hardens the first one-PC Wi-Fi/DDP MVP.

Runtime path:

    HyperHDR 22
        |
        | DDP / UDP 4048
        v
    static UDP RX buffer
        |
        v
    DdpAssembler
        |
        | newest complete frame
        v
    FrameMailbox
        |
        v
    LedRenderer
        |
        v
    PARLIO x4

## Current hardware target

- ESP32-C6
- 780 logical RGB LEDs
- TOP 230
- RIGHT 160
- BOTTOM 230
- LEFT 160
- four synchronized PARLIO lanes

## DDP runtime

Supported subset:

- DDP v1
- UDP port 4048
- RGB type 0x0B
- destination 1
- sequence 1..15
- exactly 2340 RGB bytes per complete frame
- one HyperHDR sender

No application heap allocation is performed per UDP datagram.

## Backlog handling

A temporary queue of old UDP frames is actively collapsed.

- up to 128 datagrams can be drained in one poll
- each poll has a 3 ms CPU budget
- multiple complete frames in one poll collapse to the newest frame
- if the socket still appears backlogged, up to four LED renders may be skipped while the receiver catches up
- render starvation is bounded

The goal is to drop historical frames instead of converting a short stall into growing Ambilight latency.

## Latency metrics

The controller tracks allocation-free internal frame-age percentiles:

- p50
- p95
- p99
- overflow above 128 ms
- maximum observed age

This measures DDP completion inside C6 to renderer consumption. It is not network RTT.

## HyperHDR setup

Use one DDP device:

- target: ESP32-C6 IPv4 address
- port: 4048
- LED count: 780
- continuous output: enabled

Keep only one sender active during this stage.

## Wi-Fi credentials

Copy include/secrets.example.h to include/secrets.h and set:

    AMBILIGHT_WIFI_SSID
    AMBILIGHT_WIFI_PASSWORD

Wi-Fi power saving is disabled for lower latency/jitter.

## Build

Firmware:

    pio run -e esp32-c6-devkitc-1

Native tests:

    pio test -e native

## Acceptance before USB work

At minimum:

- 60 FPS DDP works on all four lanes
- 2-hour interactive test passes
- 24-hour soak shows no progressive heap or latency growth
- p95 internal age stays below one 60 FPS frame under normal conditions
- HyperHDR/AP restart recovery works
- no partial/malformed frame is rendered
- mapping errors remain zero

USB/AWA is intentionally the next major transport only after this path is proven on hardware.
