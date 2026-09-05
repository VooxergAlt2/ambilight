# Architecture

## Current scope

Stage 2 introduces the transport-independent RGB frame core on top of the Stage 1 four-lane PARLIO engine.

The firmware still contains no Wi-Fi, DDP, USB/AWA, VL53L5CX, Web UI, OTA, source arbitration, or multi-PC logic.

## Logical frame contract

Every future input transport must publish one complete logical frame:

    RgbFrame
      generation
      receivedUs
      pixels[780] as RGB888

The RGB payload is exactly 2340 bytes.

Transport code is not allowed to address PARLIO lanes directly.

## Frame mailbox

FrameMailbox owns one published frame and protects task-level copies with a FreeRTOS mutex.

This intentionally chooses simple ownership over zero-copy complexity:

- producer copies one complete frame into the mailbox
- mailbox assigns a monotonically increasing generation
- renderer copies only when the generation changed
- no RGB FIFO is created
- future realtime transport policy remains "latest frame wins"

A 2340-byte copy is cheap enough that this can be benchmarked before considering a more complex ownership scheme.

## Fixed logical geometry

| Segment | Logical range | LEDs | PARLIO lane |
| --- | ---: | ---: | ---: |
| TOP | 0..229 | 230 | 0 |
| RIGHT | 230..389 | 160 | 1 |
| BOTTOM | 390..619 | 230 | 2 |
| LEFT | 620..779 | 160 | 3 |

Geometry is compile-time validated to ensure:

- no gaps between logical segments
- the final logical index is exactly 779
- no physical segment exceeds the 230-slot PARLIO lane
- every lane index is valid

## Segment mapping

SegmentMapper is pure C++ and converts a logical LED index into:

    { lane, physical index, valid }

Reversal is handled here, not in HyperHDR and not in the PARLIO hardware layer.

The mapper has native unit tests for every segment boundary, out-of-range access, and reversed segments.

## PARLIO hardware

LiteLEDpioGroup requires equal lane length. The group uses 230 physical positions per lane:

- lane 0: TOP, 230 real LEDs
- lane 1: RIGHT, 160 real LEDs + 70 untouched virtual positions
- lane 2: BOTTOM, 230 real LEDs
- lane 3: LEFT, 160 real LEDs + 70 untouched virtual positions

The virtual tail positions are cleared at hardware initialization and never addressed by LedRenderer.

## Renderer

LedRenderer is the only logical-frame consumer allowed to write LED pixels.

Data flow now used even by built-in test patterns:

    test RgbFrame
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
      LedEngine
         |
         v
    LiteLED PARLIO x4

Future DDP and USB/AWA receivers must stop at FrameMailbox.

## Next stage

Stage 3 adds Wi-Fi only. Test-pattern frames remain the source so that any Wi-Fi side effects on PARLIO timing can be isolated before DDP is introduced.
