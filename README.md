# ESP32-C6 Ambilight Controller

Custom ESP32-C6 Ambilight endpoint for HyperHDR.

## Current development line

Stage 23 adds runtime/NVS ToF gain calibration.

The current software stack now includes:

- Wi-Fi/DDP runtime transport
- NVS/serial Wi-Fi provisioning
- 780-pixel logical renderer
- 4 synchronized PARLIO outputs
- slow VL53L5CX wall-plane geometry
- cumulative plane deadband
- exact per-LED wall-distance correction
- persistent DISABLED / SHADOW / ACTIVE modes
- persistent global output brightness
- persistent runtime ToF calibration curve

USB/AWA remains preserved separately in:

    stage/07-usb-awa

## LED layout

- TOP: 230
- RIGHT: 160
- BOTTOM: 230
- LEFT: 160
- total: 780

## Wi-Fi

Startup priority:

    NVS -> compile-time secrets.h -> disabled

Commands:

    w<Enter>                 status
    wSSID|PASSWORD<Enter>    save + reconnect
    wclear<Enter>            clear NVS

DDP listens on UDP/4048.

## ToF geometry

VL53L5CX:

- 8x8
- internal ranging 1 Hz
- one processed pose about every 12 s

Wall model:

    z_wall = intercept + slope_x*x + slope_y*y

Per LED:

    distance_i = z_wall(x_i, y_i) - z_led_i
    gain_i = curve(distance_i)

## Plane deadband

Current threshold:

    10 mm maximum wall-position change over screen corners

Below threshold:

- no 780-value rebuild
- freshness only

At/above threshold:

- accept plane
- rebuild all 780 distances/gains

Skipped poses do not move the reference, so slow motion accumulates.

## Correction modes

    !0 = DISABLED
    !1 = SHADOW
    !2 = ACTIVE
    m  = status

Default:

    SHADOW

Entering ACTIVE starts at unity and slews toward the current valid target.

Debug probe `x` is allowed only in SHADOW.

Fail-open always returns correction to original HyperHDR RGB.

## Output brightness

Persistent LiteLED group multiplier:

    0..255

Default:

    32

Commands:

    b128<Enter>
    b<Enter>

This is independent from ToF gain.

## Runtime ToF calibration

Show:

    q<Enter>

Set:

    q50:2048,500:3072,4000:4096<Enter>

Reset:

    qreset<Enter>

Curve rules:

- 2..8 points
- increasing distance
- non-decreasing gain
- gain <= 4096

Curve edits are blocked in ACTIVE.

Changing the curve immediately fail-opens the old target and forces the next valid pose to rebuild all 780 values.

Default curve is still neutral:

    50:4096
    4000:4096

Real photometric values will be entered later without reflashing firmware.

## Debug summary

    t  raw ToF map
    g  legacy band diagnostics
    p  robust wall plane
    k  legacy gain diagnostics
    s  per-LED/perimeter gain diagnostics
    c  60 s pose capture
    r  render/candidate/physical diagnostics
    x  synthetic gain probe, SHADOW only
    m  correction mode
    b  output brightness
    w  Wi-Fi
    q  ToF calibration curve

## Development strategy

Software is completed before physical validation.

Hardware later supplies:

- actual sensor orientation/mirroring
- exact sensor offsets
- exact LED geometry
- real noise/deadband tuning
- real distance-to-brightness curve

## CI

Automatic GitHub Actions are disabled while Actions quota is exhausted.

Only manual:

    workflow_dispatch
