# Architecture

## Stage 6 scope

Stage 6 hardens the one-PC DDP runtime against temporary receive backlog and adds allocation-free latency observability.

The feature set is intentionally unchanged:

- one PC
- Wi-Fi / DDP only
- 780 RGB logical LEDs
- four PARLIO outputs
- no USB/AWA
- no VL53L5CX
- no source arbitration
- no Web UI/OTA

## Realtime receive policy

A normal HyperHDR frame is two DDP datagrams, but temporary scheduler or radio stalls can leave several frames queued in the UDP socket.

Rendering every completed historical frame would convert a temporary stall into persistent visible latency.

Stage 6 therefore collapses backlog in two places.

### 1. Inside one UDP poll

DdpUdpService drains datagrams until one of three conditions:

- socket reaches EWOULDBLOCK/EAGAIN
- 128 datagrams were parsed
- 3 ms polling budget was consumed

If several complete DDP frames are assembled during that drain, only the newest complete frame is published to FrameMailbox.

Intermediate complete frames never take the mailbox mutex and never reach the renderer.

### 2. Across successive polls

If a poll stops because of its time/packet budget instead of reaching an empty socket, backlogLikely is returned.

The main loop may skip LED rendering for up to four consecutive backlog polls so parsing can catch up faster than PARLIO rendering.

After four skips it renders anyway, preventing pathological network load from starving LEDs indefinitely.

This encodes the system rule:

    newest frame > historical frame completeness

## Why this matters

At 60 FPS a frame arrives every 16.7 ms while a 230-pixel WS2812 lane takes about 6.9 ms to transmit.

The receiver normally has ample time.

Backlog handling is therefore primarily a recovery mechanism for temporary stalls, not normal scheduling.

## Latency histogram

The main loop measures internal age:

    last DDP packet completing frame
        ->
    renderer starts consuming published frame

A fixed-memory histogram records buckets:

- <=0.25 ms
- <=0.5 ms
- <=1 ms
- <=2 ms
- <=4 ms
- <=8 ms
- <=16 ms
- <=32 ms
- <=64 ms
- <=128 ms
- overflow

Runtime diagnostics expose approximate p50/p95/p99 upper bounds plus overflow count and absolute max age.

This does not measure PC-to-ESP network latency. It measures queue/scheduling delay inside the controller, which is the part firmware can actually control.

## Heap policy

The packet path uses:

- one static 1536-byte UDP RX buffer
- one fixed DDP staging frame
- one 293-byte coverage bitmap
- fixed RgbFrame objects
- no application malloc/free per frame or datagram

The Wi-Fi/lwIP stack itself still owns normal network buffers internally.

## Stress acceptance

The one-PC Wi-Fi runtime should pass:

- >=2 hours at 60 FPS before first field use
- 24-hour soak before calling the DDP path stable
- no progressive frame-age growth
- no progressive heap loss
- mappingErrors = 0
- no watchdog/reset
- HyperHDR stop -> black within ~1 s
- HyperHDR restart -> immediate sequence resync
- AP restart -> automatic recovery
- temporary CPU/network stall -> backlog collapses rather than accumulating
- p95 internal frame age stays comfortably below one 16.7 ms video frame under normal conditions

## Next stage

After this DDP path is demonstrated on real hardware, USB/AWA can be implemented as a completely independent producer of the same RgbFrame contract.
