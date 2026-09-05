# Architecture

## Current scope

Stage 4 adds a pure C++ DDP parser/reassembler on top of the Wi-Fi foundation, but does not yet open UDP port 4048.

This keeps packet correctness independently testable before network callbacks can publish frames.

Still out of scope:

- runtime UDP listener
- USB/AWA
- VL53L5CX
- Web UI
- OTA
- source arbitration
- multi-PC logic

## HyperHDR DDP subset

The first implementation intentionally accepts only the exact subset needed by HyperHDR 22 for this 780-RGB-LED controller:

- DDP version 1
- sequence 1..15
- RGB data type 0x0B
- destination 1
- fixed logical payload of 2340 bytes

RGBW, arbitrary destinations, discovery, multiple senders, and arbitrary frame sizes are intentionally rejected or deferred.

HyperHDR 22 currently caps an RGB DDP datagram at 480 LEDs, so the normal 780-LED frame is expected as:

    packet A: offset 0,    1440 RGB bytes, PUSH=0
    packet B: offset 1440, 900 RGB bytes, PUSH=1

## Datagram parser

The parser validates:

- minimum 10-byte DDP header
- version
- sequence range
- RGB type
- destination
- non-empty payload
- exact UDP datagram length vs declared payload length

It exposes an immutable packet view over the original datagram.

## Reassembly

DdpAssembler owns:

- 2340-byte staging buffer
- 293-byte per-byte coverage bitmap
- active sequence
- PUSH state
- 50 ms assembly timeout
- last completed sequence
- counters

A frame is complete only when:

    PUSH has been observed
    AND
    all 2340 byte positions have been covered

This allows packet reordering, including the PUSH packet arriving before offset zero.

## Duplicate and overlap handling

Coverage is tracked per byte, not by a naive bytesReceived counter.

Therefore:

- duplicate packets do not fake completion
- identical overlap is allowed
- conflicting overlap rejects and resets the active frame

No partially assembled DDP frame can reach FrameMailbox.

## Sequence handling

HyperHDR uses sequence values 1..15.

The assembler:

- keeps packets from the same sequence together
- lets a newer sequence supersede an incomplete one
- rejects stale packets instead of rewinding
- handles the 15 -> 1 wrap

## Tests

Native tests cover:

- normal HyperHDR two-packet frame
- PUSH packet arriving first
- duplicate packet
- incomplete frame superseded by next sequence
- stale packet after completion
- 15 -> 1 sequence wrap
- assembly timeout
- unsupported type/destination
- frame bounds violation
- conflicting overlap

## Next stage

Stage 5 opens UDP/4048 for one configured PC/source and publishes only DdpAssembler::Complete frames to FrameMailbox.
