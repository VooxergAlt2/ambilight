# ESP32-C6 Ambilight Controller

Custom ESP32-C6 Ambilight endpoint for HyperHDR.

## Current development stage

Stage 5 is the first runtime MVP: one PC can drive all four physical LED sides over HyperHDR DDP/Wi-Fi.

Runtime path:

    HyperHDR 22
        |
        | DDP v1 / UDP 4048
        v
    ESP32-C6 static UDP RX buffer
        |
        v
    DdpAssembler
        |
        | complete 2340-byte frames only
        v
    FrameMailbox
        |
        | latest frame wins
        v
    LedRenderer
        |
        v
    SegmentMapper
        |
        v
    LiteLED PARLIO x4

## Why raw lwIP/BSD UDP instead of Arduino WiFiUDP

Arduino-ESP32 3.3.11 NetworkUDP::parsePacket() allocates a temporary 1460-byte heap buffer for every received datagram.

A 780-pixel HyperHDR frame normally uses two DDP datagrams. At 60 FPS that would mean roughly 120 malloc/free cycles per second in the realtime path.

Stage 5 instead uses one statically allocated 1536-byte RX buffer with a non-blocking lwIP socket.

## Supported DDP subset

- port 4048
- DDP version 1
- sequence 1..15
- RGB type 0x0B
- destination 1
- exactly 780 RGB LEDs / 2340 bytes
- one PC/sender for the current stage

HyperHDR 22 normally sends the frame as 1440 bytes + 900 bytes.

## HyperHDR setup

Configure one LED device:

- protocol/device: DDP
- target: ESP32-C6 IPv4 address
- port: 4048
- LED count: 780
- continuous output: enabled

Keep only one HyperHDR sender active during Stage 5.

## Wi-Fi credentials

Copy include/secrets.example.h to include/secrets.h and set:

    AMBILIGHT_WIFI_SSID
    AMBILIGHT_WIFI_PASSWORD

The real secrets file is ignored by Git.

Wi-Fi modem power saving is disabled for lower realtime jitter.

## Runtime behavior

- only complete DDP frames are published
- multiple completed frames received before rendering collapse to the newest mailbox generation
- the UDP socket is drained in bounded batches of up to 32 datagrams
- malformed/incomplete frames never reach LEDs
- if no complete DDP frame arrives for 1 second, the controller publishes one black frame
- a new valid DDP frame immediately resumes normal output
- Wi-Fi reconnect does not reboot the LED engine

## Metrics

A compact runtime line is emitted every 10 seconds with:

- Wi-Fi state/RSSI
- DDP datagrams and complete frames
- rejected/stale/timed-out/superseded frames
- sender IPv4/port
- renderer frames/mapping errors
- mailbox frame age
- max PARLIO show time
- idle blackouts
- heap/minimum heap

## Build

Firmware:

    pio run -e esp32-c6-devkitc-1

Native tests:

    pio test -e native

## Next stage

Stage 6 is a stress/hardening pass around this working Wi-Fi MVP. It will focus on queue/backlog behavior, reconnects, packet-loss recovery, frame-age percentiles, and longer-duration stability before USB/AWA is introduced.
