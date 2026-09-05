# Architecture

## Current stage

Stage 17 carries exact spatial gain all the way from the wall plane to each of the 780 logical LEDs.

Physical gain application remains disabled.

## Exact spatial chain

    VL53L5CX 8x8
      -> robust wall plane
      -> segment endpoint wall distances
      -> linear distance for each logical LED
      -> distance-to-gain curve per LED
      -> PerimeterGainSnapshot.logicalGainQ12[780]
      -> TofRenderGainBridge
      -> RenderGainContext.logicalGainQ12[780]
      -> RenderGainController per-pixel slew
      -> LedRenderer logical-index lookup
      -> ShadowRenderPolicy
      -> ORIGINAL RGB

## Why per-pixel field

The geometry gives linear distance along a segment.

The calibration law is piecewise linear in distance.

Composing those functions is piecewise linear in segment position, not necessarily one straight gain line from segment start to end.

The exact per-pixel field preserves all calibration knots.

## Rate domains

ToF measurement/model:

    ~10 Hz

Large spatial snapshot polling:

    20 Hz

Effective per-pixel gain slew/render:

    up to ~60 Hz

The 20 Hz polling rate avoids repeatedly copying a ~1.6 KB field under the ToF mutex when the sensor itself only updates around 10 Hz.

## Memory

One logical field is:

    780 * 2 bytes = 1560 bytes

Several cached/controller copies consume only a few kilobytes and avoid complex dynamic allocation.

## Screen-space invariants

Logical gain index remains independent from physical lane/index reversal.

The screen-space geometry controls which wall point belongs to which logical LED.

SegmentMapper controls wiring only.

## Safety

Any invalid/stale spatial source returns a full unity field.

Physical output is still hardwired to original RGB by ShadowRenderPolicy.
