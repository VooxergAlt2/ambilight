# Architecture

## Current stage

Stage 19 uses a slow, physically explicit wall-plane model.

Physical gain application remains disabled.

## RGB path

    HyperHDR
      -> Wi-Fi / DDP UDP
      -> DdpAssembler
      -> FrameMailbox
      -> cached RGB frame
      -> LedRenderer
      -> PARLIO
      -> 4 LED lanes

## ToF path

    VL53L5CX 8x8
      -> perpendicular distance_mm (Z)
      -> ST zone-center pitch/yaw -> X/Y
      -> robust wall plane
      -> 780 logical LED screen points
      -> +Z ray / wall-plane intersection per LED
      -> 780 LED-wall distances
      -> distance-to-gain curve per LED
      -> PerimeterGainSnapshot.logicalGainQ12[780]
      -> RenderGainController per-pixel slew
      -> shadow render only

## Wall-plane equation

    z_wall = intercept + slope_x*x + slope_y*y

For LED point:

    P_led = (x_led, y_led, z_led)

the corresponding wall spot is:

    P_wall = (x_led, y_led, z_wall)

and throw distance is:

    distance = z_wall - z_led

This is deliberate screen-normal projection, not shortest orthogonal distance to the tilted wall.

## Why no extrapolation-confidence ratio

The fitted plane is the geometric model of the wall.

Once that plane passes its own validity checks, screen points are evaluated from the plane directly. The size of the wall patch directly observed by the ToF sensor is useful diagnostics for the plane fit, but it is not a separate gain/projection confidence term.

## Rate domains

RGB / DDP:

    realtime

VL53L5CX internal ranging:

    1 Hz

Wall-plane transfer and processing:

    about every 12 seconds

Large gain target polling:

    1 Hz

Effective gain slew/render after a target update:

    up to about 60 Hz

The slow ToF cadence is intentional because TV pose changes are rare compared with video frames.

## Memory

One exact logical gain field:

    780 * 2 bytes = 1560 bytes

Several fixed-size copies remain inexpensive on ESP32-C6 and avoid dynamic allocation.

## Screen-space invariant

Logical screen geometry is independent from physical strip reversal.

Wall projection always uses logical screen coordinates. SegmentMapper handles wiring only.

## Safety

Invalid/stale plane or an out-of-range calculated LED-wall distance resolves the complete gain field to unity.

Physical output remains original RGB through ShadowRenderPolicy.
