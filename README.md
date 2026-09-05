# ESP32-C6 Ambilight Controller

Custom ESP32-C6 Ambilight endpoint for HyperHDR.

## Current development line

Stage 21 adds persistent runtime output brightness on top of Stage 20 correction modes and Stage 19 slow wall-plane geometry.

Active frame transport remains Wi-Fi/DDP from one PC.

USB/AWA work remains preserved separately in:

    stage/07-usb-awa

## RGB path

    HyperHDR
      -> DDP UDP/4048
      -> complete RGB frame
      -> logical 780-pixel render
      -> correction/output policy
      -> 4 synchronized PARLIO lanes

Logical LED count:

- TOP: 230
- RIGHT: 160
- BOTTOM: 230
- LEFT: 160
- total: 780

## ToF geometry

VL53L5CX is a slow TV-pose sensor.

Normal operation:

- 8x8
- internal ranging: 1 Hz
- one transferred/processed pose frame: about every 12 s

Wall plane:

    z_wall = intercept + slope_x*x + slope_y*y

Per logical LED:

    distance_i = z_wall(x_i, y_i) - z_led_i
    gain_i = curve(distance_i)

## Plane deadband

A fresh plane is compared with the last applied plane at the LED rectangle corners.

Current threshold:

    10 mm maximum predicted wall-position change

Below threshold, only freshness is updated.

At/above threshold, all 780 wall distances/gains are rebuilt.

The comparison is cumulative against the last applied plane.

## Correction modes

Persistent NVS mode:

    !0 = DISABLED
    !1 = SHADOW
    !2 = ACTIVE
    m  = mode status

Default:

    SHADOW

Entering ACTIVE starts from unity and slews toward the target.

The synthetic `x` probe is allowed only in SHADOW.

Fail-open always resolves physical correction to original RGB.

## Global output brightness

Persistent range:

    0..255

Default:

    32

Set:

    b128<Enter>

Read:

    b<Enter>

Brightness is a separate final LiteLED group multiplier and is not part of the ToF calibration curve.

Changing it does not reinitialize PARLIO. The latest RGB frame is rerendered once.

## Gain dynamics

Accepted geometry changes create a new 780-value target.

Effective gains slew at:

    8192 Q12 / second

Gain-only rerenders are capped around 60 Hz.

Main polls the ToF target at 1 Hz.

## Current ToF calibration

Default distance curve remains neutral:

    50 mm   -> 100%
    4000 mm -> 100%

Real coefficients will be supplied during later physical calibration.

## Debug

    t  raw ToF map
    g  legacy LEFT/CENTER/RIGHT diagnostics
    p  wall plane
    k  legacy gain snapshot
    s  perimeter/per-LED wall-distance gains
    c  60 s pose calibration capture
    r  render/candidate/physical diagnostics
    x  10 s synthetic probe, SHADOW only
    m  correction mode / persistence
    bN output brightness 0..255

## Development strategy

The software project is completed before physical validation.

Synthetic/contract tests define geometry, projection, deadband, correction-mode and fail-open behavior.

Hardware work later supplies real mounting, noise and photometric calibration data.

## CI

Automatic GitHub Actions are disabled while Actions quota is exhausted.

The workflow is manual only:

    workflow_dispatch
