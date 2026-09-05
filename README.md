# ESP32-C6 Ambilight Controller

Custom ESP32-C6 Ambilight endpoint for HyperHDR.

## Current development line

Stage 28 adds temporary LED commissioning patterns on top of runtime lane/direction mapping.

The firmware stack now includes:

- Wi-Fi/DDP runtime transport
- one active DDP sender lease
- NVS/serial Wi-Fi provisioning
- 780-pixel logical renderer
- 4 synchronized PARLIO outputs
- slow VL53L5CX wall-plane geometry
- cumulative plane deadband
- exact per-LED wall-distance correction
- persistent DISABLED / SHADOW / ACTIVE modes
- persistent output brightness
- persistent runtime ToF gain curve
- persistent runtime spatial profile

USB/AWA remains preserved separately in:

    stage/07-usb-awa

## LED layout

- TOP: 230
- RIGHT: 160
- BOTTOM: 230
- LEFT: 160
- total: 780

## DDP transport

Port:

    UDP/4048

The first structurally valid DDP sender acquires the stream lease.

Sender identity:

    IPv4 + UDP source port

Default lease:

    1 second

While locked:

    owner DDP   -> assembler
    other DDP   -> dropped

Malformed UDP/DDP cannot acquire or extend the lease.

After lease timeout, assembler partial state and sequence history are reset before a new sender can acquire ownership.

Rejected sender traffic cannot by itself trigger RGB render deferral.

STAT reports:

    sender_lock
    sender
    saccept
    sinvalid
    sforeign
    sacq
    srel

## Wi-Fi

    w<Enter>                 status
    wSSID|PASSWORD<Enter>    save + reconnect
    wclear<Enter>            clear NVS

Startup:

    NVS -> secrets.h fallback -> disabled

## Spatial profile

    y<Enter>
    yWIDTH,HEIGHT,SENSOR_X,SENSOR_Y,LED_Z,ROT,MIRROR,DEADBAND<Enter>
    yreset<Enter>

Default:

    y1437.5,1000,0,0,0,0,0,10

Spatial edits are blocked in ACTIVE.

## ToF geometry

VL53L5CX:

- 8x8
- internal ranging 1 Hz
- one processed pose about every 12 s

Plane:

    z_wall = intercept + slope_x*x + slope_y*y

Each LED:

    distance_i = z_wall(x_i, y_i) - z_led_i
    gain_i = curve(distance_i)

## Runtime gain calibration

    q<Enter>
    q50:2048,500:3072,4000:4096<Enter>
    qreset<Enter>

Curve edits are blocked in ACTIVE.

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

ACTIVE starts from unity and slews toward a valid target.

Fail-open returns physical correction to original HyperHDR RGB.

## Output brightness

    b<Enter>
    b128<Enter>

Range:

    0..255

Default:

    32

## Debug

    t  raw ToF map
    g  legacy band diagnostics
    p  wall plane
    k  legacy gain diagnostics
    s  per-LED/perimeter gains
    c  pose capture
    r  render diagnostics
    x  synthetic probe, SHADOW only
    m  correction mode
    b  brightness
    w  Wi-Fi
    q  gain curve
    y  spatial profile

## Development strategy

Software is completed before physical commissioning.

Hardware later provides mounting verification, noise tuning and real photometric coefficients.

## CI

Automatic GitHub Actions are disabled while Actions quota is exhausted.

Only manual:

    workflow_dispatch


## Runtime LED mapping

Physical lane assignment and strip direction can be commissioned without rebuilding firmware.

    l<Enter>                       status
    l0:0,1:0,2:0,3:0<Enter>       set TOP/RIGHT/BOTTOM/LEFT lane:reverse
    lreset<Enter>                  default

Mapping edits require:

    brightness = 0

Each lane 0..3 must be used exactly once.

GPIO pins and logical segment lengths remain compile-time constants.


## LED commissioning

Safe test brightness:

    1..64

Commands:

    i<Enter>   status
    i1         segment colors
    i2         START/MID/END direction markers
    i0         stop

Patterns run for 15 seconds, bypass ToF correction for the test frame, and then restore the newest HyperHDR frame or black if no RGB source exists.
