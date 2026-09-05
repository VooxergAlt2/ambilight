# ESP32-C6 Ambilight Controller

Custom ESP32-C6 Ambilight endpoint for HyperHDR.

## Current development line

Stage 20 adds persistent runtime correction modes on top of the Stage 19 slow wall-plane model.

Active frame transport remains Wi-Fi/DDP from one PC.

USB/AWA work remains preserved separately in:

    stage/07-usb-awa

## RGB path

    HyperHDR
      -> DDP UDP/4048
      -> complete RGB frame
      -> logical 780-pixel render
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
- sensor internal ranging: 1 Hz
- one transferred/processed pose frame: about every 12 s

VL53L5CX `distance_mm` is treated as perpendicular Z.

X/Y are reconstructed using ST zone-center geometry, then a robust wall plane is fitted:

    z_wall = intercept + slope_x*x + slope_y*y

For every logical LED:

    distance_i = z_wall(x_i, y_i) - z_led_i
    gain_i = curve(distance_i)

So all 780 LED wall distances/gains are derived from the plane directly.

## Plane deadband

A fresh plane does not automatically rebuild the field.

The candidate is compared with the last applied plane at the LED-rectangle corners.

Current threshold:

    10 mm maximum predicted wall-position change

Below threshold:

- refresh source timestamp/generation
- keep existing 780 distances/gains

At or above threshold:

- accept new plane
- rebuild all 780 distances
- rebuild all 780 gains

The comparison is cumulative against the last applied plane.

## Correction modes

Runtime correction mode is stored in NVS.

    !0 = DISABLED
    !1 = SHADOW
    !2 = ACTIVE
    m  = status

Default:

    SHADOW

DISABLED leaves HyperHDR RGB unchanged and suppresses gain-driven renderer work.

SHADOW calculates complete correction and diagnostics but sends original RGB to LEDs.

ACTIVE sends the corrected candidate RGB to LEDs. Invalid/stale geometry still fails open to unity/original RGB.

Entering ACTIVE starts from unity and slews toward the target rather than applying an old shadow profile abruptly.

The synthetic `x` probe is allowed only in SHADOW.

## Gain dynamics

Material geometry changes produce a new 780-value target.

Effective gains then slew at:

    8192 Q12 / second

Gain-only rerenders are capped around 60 Hz.

Main polls the ToF target field at 1 Hz.

## Current calibration

The default distance curve is intentionally neutral:

    50 mm   -> 100%
    4000 mm -> 100%

The software pipeline is complete enough to support ACTIVE, but real photometric coefficients will be filled during later physical calibration.

## Debug

    t  raw ToF map
    g  legacy LEFT/CENTER/RIGHT diagnostics
    p  wall plane
    k  legacy gain snapshot
    s  per-LED/perimeter wall-distance gains
    c  60 s pose calibration capture
    r  render/candidate/physical diagnostics
    x  10 s synthetic probe, SHADOW only
    m  correction mode / persistence

## Development strategy

Hardware validation is deferred until the software project is functionally complete.

Synthetic and contract tests cover geometry, plane fitting, deadband, projection, gain math, correction modes and fail-open behavior.

Hardware testing later supplies:

- actual sensor rotation/mirroring
- exact sensor X/Y/Z offsets
- exact LED geometry
- real sensor noise/deadband tuning
- real distance-to-brightness calibration points

## CI

Automatic GitHub Actions are disabled while repository Actions quota is exhausted.

The workflow is manual only:

    workflow_dispatch
