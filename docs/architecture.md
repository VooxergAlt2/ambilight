# Architecture

## Current stage

Stage 20 combines:

- realtime Wi-Fi/DDP RGB transport
- slow VL53L5CX wall-plane geometry
- exact 780-value spatial gain field
- cumulative geometry deadband
- persistent DISABLED / SHADOW / ACTIVE correction modes

## RGB path

    HyperHDR
      -> Wi-Fi / DDP UDP/4048
      -> DdpAssembler
      -> FrameMailbox
      -> cached RgbFrame
      -> RenderScheduler
      -> RenderGainController
      -> LedRenderer
      -> SegmentMapper
      -> PARLIO x4

## ToF path

    VL53L5CX 8x8
      -> perpendicular distance_mm (Z)
      -> zone geometry -> X/Y
      -> robust wall plane
      -> PlaneChangeGate
          insignificant -> refresh freshness only
          material      -> rebuild
      -> 780 logical LED screen points
      -> +Z plane intersection per LED
      -> 780 wall distances
      -> distance curve per LED
      -> PerimeterGainSnapshot.logicalGainQ12[780]
      -> TofRenderGainBridge
      -> RenderGainContext.logicalGainQ12[780]

## Wall model

    z_wall = intercept + slope_x*x + slope_y*y

For one LED:

    distance = z_wall(x_led, y_led) - z_led

This is screen-normal throw distance, not shortest orthogonal distance to a tilted wall.

## Plane-change gate

The difference between two planes is linear.

Over the rectangular LED area, maximum absolute Z difference occurs at a corner.

Therefore the gate compares the four screen corners instead of scanning all 780 LED positions just to decide whether a rebuild is necessary.

Current threshold:

    10 mm

Skipped candidates do not replace the accepted reference, so slow motion accumulates until it becomes material.

## Rate domains

DDP/RGB:

    realtime

VL53L5CX internal ranging:

    1 Hz

Pose transfer / robust plane:

    about every 12 s

Per-pixel field rebuild:

    only on material accepted plane change

Main target polling:

    1 Hz

Gain slew and gain-only rerender:

    up to about 60 Hz while moving toward target

## Correction output policy

    DISABLED:
        original RGB
        gain pipeline ignored by renderer scheduling

    SHADOW:
        calculate candidate
        original RGB to physical output

    ACTIVE:
        calculate candidate
        candidate RGB to physical output

ACTIVE still depends on RenderGainContext usability. Fail-open is unity, so physical output returns to original RGB.

Mode is persisted in NVS and defaults to SHADOW.

## Screen-space invariant

All geometry/gain indices are logical HyperHDR indices.

Physical wiring reversal occurs later in SegmentMapper and cannot reverse the mathematical wall correction.

## Memory

One Q12 gain field:

    780 * 2 = 1560 bytes

Fixed-size cached copies are deliberately used instead of dynamic allocation.

## Recovery independence

ToF failure does not restart or block DDP.

Wi-Fi reconnect does not reset ToF.

Invalid/stale ToF only changes correction state to fail-open unity.

## Validation strategy

Software functionality is completed using deterministic native tests and synthetic sensor contracts.

Physical hardware is a later validation/calibration stage, not a prerequisite for implementing the remaining firmware.
