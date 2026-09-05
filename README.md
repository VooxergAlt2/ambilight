# ESP32-C6 Ambilight Controller

Custom ESP32-C6 Ambilight endpoint for HyperHDR.

## Current development line

Stage 22 adds runtime Wi-Fi provisioning on top of persistent correction modes, runtime output brightness and slow ToF wall-plane geometry.

Active transport remains Wi-Fi/DDP from one PC.

USB/AWA work remains preserved separately in:

    stage/07-usb-awa

## RGB path

    HyperHDR
      -> DDP UDP/4048
      -> complete RGB frame
      -> logical 780-pixel render
      -> correction/output policy
      -> 4 synchronized PARLIO lanes

LED layout:

- TOP: 230
- RIGHT: 160
- BOTTOM: 230
- LEFT: 160
- total: 780

## Wi-Fi configuration

Credential priority at boot:

    NVS
      ↓
    compile-time secrets.h fallback
      ↓
    disabled

Runtime commands:

    w<Enter>                 status
    wSSID|PASSWORD<Enter>    save + reconnect immediately
    wclear<Enter>            clear NVS credentials

DDP UDP/4048 is started dynamically if Wi-Fi is provisioned after boot.

Passwords are never printed by firmware diagnostics.

## ToF geometry

VL53L5CX operates as a slow TV-pose sensor:

- internal ranging: 1 Hz
- transferred/processed pose: about every 12 s
- robust 8x8 wall-plane fit

Plane:

    z_wall = intercept + slope_x*x + slope_y*y

Per LED:

    distance_i = z_wall(x_i, y_i) - z_led_i
    gain_i = curve(distance_i)

## Plane deadband

The candidate plane is compared with the last applied plane at screen corners.

Current threshold:

    10 mm maximum wall-position change

Below threshold:

- refresh freshness only
- keep existing 780-value field

At/above threshold:

- accept plane
- rebuild 780 distances/gains

The deadband is cumulative against the last applied plane.

## Correction modes

Persistent NVS mode:

    !0 = DISABLED
    !1 = SHADOW
    !2 = ACTIVE
    m  = status

Default:

    SHADOW

Entering ACTIVE starts from unity and slews toward the target.

The synthetic `x` probe is allowed only in SHADOW.

Fail-open always resolves correction to original RGB.

## Global output brightness

Persistent:

    0..255

Default:

    32

Commands:

    b128<Enter>   set
    b<Enter>      status

Brightness is a final global LiteLED multiplier, independent from ToF correction.

## Gain dynamics

Accepted geometry changes create a new 780-value target.

Effective gains slew at:

    8192 Q12 / second

Gain-only rerenders are capped around 60 Hz.

## Current ToF calibration

The default curve remains neutral:

    50 mm   -> 100%
    4000 mm -> 100%

Real coefficients will be inserted during later physical calibration.

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
    bN output brightness
    w... Wi-Fi provisioning/status

## Development strategy

The software project is completed before physical validation.

Synthetic/contract tests define geometry, projection, deadband, correction mode and fail-open behavior.

Hardware later supplies real sensor orientation/offsets, noise tuning and photometric calibration.

## CI

Automatic GitHub Actions are disabled while Actions quota is exhausted.

The workflow is manual only:

    workflow_dispatch
