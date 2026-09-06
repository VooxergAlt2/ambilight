# Runtime LED topology profile

## Purpose

Stage 38 evolves the old lane/reversal-only mapping into the authoritative
runtime LED topology.

For each logical TV side the profile stores:

- active LED count
- physical PARLIO output, surfaced as GPIO
- forward/reversed direction

No firmware rebuild is required.

## Capacity and default

Static capacity:

    4 lanes x 230 = 920 LEDs

Default topology:

    TOP     230 : GPIO18 : FWD
    RIGHT   160 : GPIO19 : FWD
    BOTTOM  230 : GPIO20 : FWD
    LEFT    160 : GPIO21 : FWD

The logical frame is contiguous in TOP, RIGHT, BOTTOM, LEFT order. Logical
starts are recomputed from configured lengths.

## Serial commands

Status:

    l<Enter>

Set:

    lCOUNT:GPIO:REV,COUNT:GPIO:REV,COUNT:GPIO:REV,COUNT:GPIO:REV<Enter>

Default example:

    l230:18:0,160:19:0,230:20:0,160:21:0<Enter>

Reset:

    lreset<Enter>

## Validation

For every side:

    COUNT  1..230
    GPIO   18 | 19 | 20 | 21
    REV    0 | 1

Every GPIO must be used exactly once.

Topology edits require:

    brightness = 0

## Coordinated runtime behavior

A valid topology apply coordinates all dependent subsystems:

1. ToF receives the new LED sampling topology
2. DDP expected frame size becomes totalLedCount * 3
3. sender lease and assembler sequence epoch reset
4. LedRenderer switches logical-to-physical mapping
5. disabled-pixel entries outside shortened sides are cleared
6. cached RGB is replaced with a black frame carrying the new pixelCount
7. gain state returns fail-open/unity until a fresh ToF projection exists

This avoids mixed old/new topology frames.

## Physical GPIO model

PARLIO is initialized on four fixed physical outputs:

    lane 0 -> GPIO18
    lane 1 -> GPIO19
    lane 2 -> GPIO20
    lane 3 -> GPIO21

The commissioning UI exposes GPIO numbers, not internal lane numbers.

Raw GPIO tests can directly light a physical lane before logical side
assignment is trusted.

## ToF independence from wiring direction

ToF distance/gain is calculated in logical screen order.

Physical GPIO assignment and REV are applied afterward by SegmentMapper.
Therefore rewiring or reversing a strip cannot reverse the wall model.

## NVS

Namespace:

    ambilight

Keys:

    led_map
    led_map_ver

Schema:

    2

The blob is written first and the version commit marker last.

A stale/incompatible mapping schema falls back to the Stage 38 default
topology and can then be recommissioned from the web UI.
