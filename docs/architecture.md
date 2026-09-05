# Architecture

## Stage 5 scope

Stage 5 is the first end-to-end HyperHDR runtime for one PC:

    HyperHDR
      |
      | DDP / UDP 4048
      v
    DdpUdpService
      |
      v
    DdpAssembler
      |
      | complete RgbFrame only
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
    LedEngine
      |
      v
    PARLIO x4

USB/AWA, ToF, source arbitration, Web UI, OTA, and multi-PC behavior remain explicitly out of scope.

## UDP implementation

The runtime intentionally does not use Arduino NetworkUDP/WiFiUDP.

Arduino-ESP32 3.3.11 NetworkUDP::parsePacket() allocates a 1460-byte temporary heap buffer for every UDP datagram. HyperHDR emits roughly two datagrams per RGB frame, so at 60 FPS this becomes about 120 temporary allocations per second.

DdpUdpService therefore uses an ESP-IDF/lwIP BSD UDP socket:

- AF_INET / SOCK_DGRAM
- bound to INADDR_ANY:4048
- static 1536-byte receive buffer
- non-blocking MSG_DONTWAIT receive
- requested 32 KiB socket RX queue
- at most 32 datagrams drained per application poll

No allocation is performed per DDP datagram by application code.

## Backlog policy

DdpUdpService may publish more than one complete frame during a single poll.

FrameMailbox stores only the latest publication generation from the point of view of the renderer. The main loop renders once after the socket drain.

Therefore, if several complete frames accumulated while PARLIO was busy, intermediate completed frames are intentionally skipped.

This preserves the core realtime policy:

    drop intermediate frames before accumulating visible latency

The socket drain is bounded to 32 datagrams so a large backlog cannot monopolize the single ESP32-C6 core forever.

## Single sender assumption

Stage 5 intentionally has no sender lease or multi-host arbitration.

The most recent sender address is recorded for diagnostics only.

Only one HyperHDR instance/PC is allowed to transmit to the controller during this stage.

## Idle behavior

The LED engine starts black.

Once at least one valid DDP frame has been seen, a one-second absence of complete DDP frames causes one synthetic black frame to be published.

The black frame goes through the normal FrameMailbox and LedRenderer path.

A later complete DDP frame clears the idle state immediately.

## Observability

Runtime diagnostics expose both transport and rendering state.

Transport:
- UDP datagrams/bytes
- completed frames
- socket errors
- publication failures
- current sender
- DDP reject/stale/timeout/supersede counters

Renderer:
- rendered frame count
- mapping errors
- latest internal frame age
- maximum internal frame age
- maximum PARLIO show time

System:
- Wi-Fi/RSSI
- free heap
- minimum free heap

## Stage 5 acceptance

With one HyperHDR sender:

- controller receives DDP on UDP/4048
- 780 logical LEDs map to all four physical lanes
- 60 FPS video runs continuously
- no malformed or partial frame is rendered
- mappingErrors remains zero
- frame age remains bounded rather than growing over time
- stopping HyperHDR blanks the LEDs within about one second
- restarting HyperHDR resumes without C6 reboot
- AP/router reconnect recovers without restarting the LED engine
- no progressive heap loss during a multi-hour run

## Next stage

Stage 6 hardens this path under sustained load and faults before any second transport is introduced.
