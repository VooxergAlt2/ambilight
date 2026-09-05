# ESP32-C6 Ambilight Controller

Custom ESP32-C6 Ambilight endpoint for HyperHDR.

## Current development line

Stage 24 adds a persistent runtime spatial profile.

The firmware now supports without reflashing:

- Wi-Fi provisioning
- output brightness
- correction mode
- ToF distance/gain curve
- screen/sensor spatial geometry
- sensor rotation/mirroring
- plane deadband

Active transport remains Wi-Fi/DDP from one PC.

USB/AWA remains preserved separately in:

    stage/07-usb-awa

## LED layout

Logical LEDs:

- TOP: 230
- RIGHT: 160
- BOTTOM: 230
- LEFT: 160
- total: 780

## Wi-Fi

    w<Enter>                 status
    wSSID|PASSWORD<Enter>    save + reconnect
    wclear<Enter>            clear NVS

Startup priority:

    NVS -> secrets.h fallback -> disabled

DDP listens on UDP/4048.

## ToF spatial profile

Show:

    y<Enter>

Set:

    yWIDTH,HEIGHT,SENSOR_X,SENSOR_Y,LED_Z,ROT,MIRROR,DEADBAND<Enter>

Default:

    y1437.5,1000,0,0,0,0,0,10

Reset:

    yreset<Enter>

All mounting-dependent ToF geometry is versioned and stored in NVS.

A profile change is blocked in ACTIVE mode and fail-opens the old spatial correction until the next valid pose is rebuilt with the new profile.

## ToF geometry model

VL53L5CX:

- 8x8
- internal ranging 1 Hz
- one processed pose about every 12 s

The sensor distance is perpendicular Z.

A robust wall plane is fitted:

    z_wall = intercept + slope_x*x + slope_y*y

For every logical LED:

    distance_i = z_wall(x_i, y_i) - z_led_i
    gain_i = curve(distance_i)

## Plane deadband

Default:

    10 mm maximum wall-position change at screen corners

Below threshold:

- refresh freshness
- no 780-value rebuild

At/above threshold:

- accept new plane
- rebuild 780 distances/gains

The reference is cumulative against the last applied plane.

## Runtime gain calibration

    q<Enter>                                  status
    q50:2048,500:3072,4000:4096<Enter>       set
    qreset<Enter>                             default

Edits are blocked in ACTIVE mode.

Default remains neutral:

    50:4096
    4000:4096

## Correction modes

    !0 = DISABLED
    !1 = SHADOW
    !2 = ACTIVE
    m  = status

Default:

    SHADOW

ACTIVE starts from unity and slews toward the valid target.

Fail-open resolves to original HyperHDR RGB.

Synthetic probe `x` is SHADOW-only.

## Output brightness

    b<Enter>      status
    b128<Enter>   set 0..255

Default:

    32/255

This is an independent final LiteLED group multiplier.

## Debug

    t  raw ToF map
    g  legacy band diagnostics
    p  robust wall plane
    k  legacy gain diagnostics
    s  per-LED/perimeter gains
    c  60 s pose capture
    r  render/candidate/physical diagnostics
    x  synthetic probe, SHADOW only
    m  correction mode
    b  global brightness
    w  Wi-Fi provisioning
    q  gain curve
    y  spatial profile

## Development strategy

The firmware is being completed before physical validation.

Hardware later becomes commissioning/calibration rather than a prerequisite for coding.

## CI

Automatic GitHub Actions are disabled while Actions quota is exhausted.

Only manual:

    workflow_dispatch
