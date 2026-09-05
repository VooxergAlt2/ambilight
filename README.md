# ESP32-C6 Ambilight Controller

Custom ESP32-C6 Ambilight endpoint for HyperHDR.

## Current stage

Stage 19 keeps active transport Wi-Fi/DDP and treats VL53L5CX as a slow TV-pose sensor rather than a realtime video sensor.

Normal ToF processing happens about every 12 seconds. The sensor remains initialized, ranges internally at 1 Hz, and only one 8x8 frame is transferred and processed per pose interval.

Physical ToF gain application is still disabled. Physical LEDs receive original HyperHDR RGB.

## Wall model

VL53L5CX `distance_mm` is treated as ST's perpendicular Z distance.

The estimator reconstructs X/Y using ST's published 8x8 zone-center pitch/yaw table and fits:

    z_wall = intercept + slope_x*x + slope_y*y

For every one of the 780 logical LEDs:

1. build its screen-space point
2. project a ray along screen/sensor +Z
3. intersect that ray with the fitted wall plane
4. obtain LED-to-wall throw distance
5. evaluate the distance-to-gain calibration curve

The correction field therefore remains exact inside every segment.

## No extrapolation-confidence gate

The previous Stage 16 extrapolation-ratio warning is retired.

Once the wall plane itself is valid, LED-wall distances are defined by that plane. The directly observed ToF footprint does not scale, modify, or reject the projected perimeter.

Projection fails open only when the wall plane is invalid/stale or a calculated LED-wall distance is outside the configured physical range.

## Plane-change deadband

A fresh plane does not automatically rebuild the 780-value field.

The candidate plane is compared with the last plane that actually changed the applied field. Both planes are evaluated at the LED-rectangle corners.

Current threshold:

    10 mm maximum wall-position change

Below 10 mm, only freshness metadata is updated. The existing 780 distances/gains remain untouched.

The comparison is cumulative against the last applied plane, so slow movement made of several small steps eventually crosses the threshold and triggers a rebuild.

## Rate domains

- HyperHDR / DDP RGB: realtime
- ToF sensor internal ranging: 1 Hz
- ToF wall-pose processing: about every 12 s
- accepted plane changes: rebuild 780 distances/gains only when needed
- renderer target polling: 1 Hz
- gain slew / gain-only rerender after a target change: up to about 60 Hz

## Debug commands

    t
raw ToF map

    g
legacy LEFT/CENTER/RIGHT geometry

    p
robust 2D wall plane, yaw/pitch and residual quality

    k
legacy ToF gain snapshot

    s
perimeter wall distances and shadow gains

    c
60-second slow pose calibration capture

    r
renderer shadow + target/effective + scheduler stats

    x
10-second aggressive shadow probe

## Calibration capture

The capture records plane yaw/pitch/intercept and wall distances derived from the plane for TOP/RIGHT/BOTTOM/LEFT.

At the end it prints p10/median/p90 summaries. With the normal ~12-second pose interval, a 60-second capture yields several independent measurements without continuously loading the MCU or I2C bus.

## USB/AWA

Preserved separately in:

    stage/07-usb-awa

Active development remains Wi-Fi/DDP.

## Development strategy

The firmware is developed to software-complete state before hardware validation.

Synthetic/contract tests define geometry, deadband, projection and fail-open behavior. Hardware work later validates mounting orientation/noise and supplies real photometric curve coefficients.

## Safety

The shipping distance-to-gain curve is still unity.

Even if a non-unity shadow profile is produced, `ShadowRenderPolicy` still sends original HyperHDR RGB to the physical LEDs.
