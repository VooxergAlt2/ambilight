# Runtime LED topology profile

## Purpose

Stage 38 introduced the authoritative runtime LED topology. Stage 39 hardens
its transition and persistence behavior before flashing.

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

Topology edits use a controller-owned safety transaction:

    cancel active commissioning
    controlled physical blackout
    apply/persist topology
    rollback on failure
    restore output brightness

The operator does not need to set brightness to 0.

## Coordinated runtime behavior

A valid topology apply coordinates all dependent subsystems:

1. ToF receives the new LED sampling topology
2. DDP expected frame size becomes totalLedCount * 3
3. sender lease and assembler sequence epoch reset
4. LedRenderer switches logical-to-physical mapping and clears all fixed
   physical lane buffers when the mapping actually changes
5. disabled-pixel entries outside shortened sides are prepared in memory
6. topology persistence/reset is committed
7. any required disabled-pixel sanitation is persisted only after topology
   commit
8. cached RGB is replaced with a black frame carrying the new pixelCount
9. gain state returns fail-open/unity until a fresh ToF projection exists

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

    3

The blob is written first and the version commit marker last.

Reset removes the version commit marker first. If later stale-blob cleanup
fails, the old blob is inert on reboot and cannot resurrect a topology the
operator already reset.

A stale/incompatible mapping schema falls back to the firmware default
topology and can then be recommissioned from the web UI.
