# ESP32-C6 Ambilight Controller

Custom ESP32-C6 Ambilight endpoint for HyperHDR.

## Current development line

Stage 33 moves runtime payload grammar for brightness, LED mapping, ToF spatial geometry and gain curves out of main.cpp into a pure typed parser.

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

## Validation

Automatic GitHub Actions are disabled while Actions quota is exhausted.

The repository workflow remains manual:

    workflow_dispatch

Local Windows validation:

    powershell -ExecutionPolicy Bypass -File tools/validate.ps1

Local Linux/macOS validation:

    bash tools/validate.sh

Each run writes native-test, firmware-build and summary logs under:

    .artifacts/validation/<timestamp>/

The Stage 31 harness has been statically reviewed but has not been executed in this development session.


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


## Factory recovery

With LEDs disabled:

    b0
    freset<Enter>

The command clears the complete `ambilight` NVS namespace and restarts only after a successful durable clear.

If NVS clear fails, runtime settings are left unchanged and the controller does not reboot.


## Render-state scheduling

A cached HyperHDR frame is rerendered when output state changes even if no new RGB arrives.

Current state-dirty sources include:

- ToF gain target/slew
- correction mode
- output brightness
- runtime LED mapping

State-only rerenders are limited to about 60 Hz.

Fresh RGB frames bypass that limiter.

Telemetry now reports:

    sched_state
    sched_state_def

instead of the old misleading gain-only names.


## Serial command architecture

Serial command bytes now pass through:

    Serial.read()
      -> SerialCommandParser
      -> typed SerialCommandEvent
      -> subsystem handler

Line commands are bounded and NUL-terminated.

If a line contains an unsupported control byte or overflows its buffer, the remainder of that line is discarded until Enter. This prevents damaged input from accidentally triggering an unrelated single-key debug command.

Framing contracts live in:

    test/test_serial_command_parser


## Typed runtime payload parsing

After serial framing, configuration payloads are parsed into existing domain objects:

    b -> uint8_t brightness
    l -> LedMappingProfile
    y -> TofSpatialProfile
    q -> GainPoint[] + DistanceGainCurve validation

The parser is pure C++ and covered by native contracts.

main.cpp now owns orchestration and subsystem actions rather than numeric command grammar.
