# ESP32-C6 Ambilight Controller

Custom ESP32-C6 Ambilight endpoint for HyperHDR.

## Current development line

Stage 38 adds a complete commissioning layer. LED topology is runtime-configurable (side lengths, GPIO assignment and direction), DDP frame size follows the active total, logical/raw GPIO range tests are available, and ToF has a transient 8x8 live-debug mode for orientation and plane commissioning.

The firmware stack now includes:

- Wi-Fi/DDP runtime transport
- one active DDP sender lease
- NVS/serial Wi-Fi provisioning
- runtime logical renderer with 920-LED static capacity
- 4 synchronized PARLIO outputs
- slow VL53L5CX wall-plane geometry with 60 s live-debug override
- cumulative plane deadband
- exact per-LED wall-distance correction
- persistent DISABLED / SHADOW / ACTIVE modes
- persistent output brightness
- persistent runtime ToF gain curve
- persistent runtime spatial profile
- minimal HTTP/80 commissioning/control UI
- persisted one-disabled-pixel-per-segment mask
- runtime LED topology: COUNT/GPIO/REV per TV side
- logical-side and raw-GPIO range commissioning tests
- normalized selectable 8x8 ToF commissioning matrix

USB/AWA remains preserved separately in:

    stage/07-usb-awa

## LED topology

Static capacity:

    4 lanes x 230 = 920 LEDs maximum

Default runtime topology:

    TOP     230 -> GPIO18 FWD
    RIGHT   160 -> GPIO19 FWD
    BOTTOM  230 -> GPIO20 FWD
    LEFT    160 -> GPIO21 FWD
    total   780

Each side may use 1..230 active addresses. The active DDP payload is always:

    totalLedCount * 3 bytes

Topology changes require brightness=0 and start a fresh DDP sender/assembly epoch.

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

## Minimal web UI

When Wi-Fi is enabled:

    http://<controller-ip>/

The page provides:

- correction mode
- brightness
- LED commissioning patterns
- runtime side length / GPIO / direction topology
- ToF spatial profile
- distance/gain curve
- 60-second calibration capture
- SHADOW probe
- Wi-Fi provisioning
- guarded factory reset
- compact DDP/ToF/runtime diagnostics
- LED logical-side/raw-GPIO range tests
- live normalized 8x8 ToF zone commissioning

Implementation constraints:

    one HTTP client
    no keep-alive
    no WebSocket
    no external assets
    no web framework
    <=512 B receive per loop
    <=1024 B send per loop

DDP is serviced before the web layer.

State-changing requests are acknowledged before they are released to main,
so Wi-Fi changes and factory reset cannot race the HTTP response.

Saved Wi-Fi passwords are never returned.

The interface is trusted-LAN only and has no login. Do not expose TCP/80 to
the public internet.

See:

    docs/web-ui.md

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
- normal processed pose about every 12 s
- explicit ToF debug processes about every 1 s for 60 s
- debug is refused in ACTIVE and is not persisted

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
    z  toggle 60 s ToF live debug

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

Stage 35 was validated locally on Windows with:

    147 native tests passed
    ESP32-C6 firmware build passed

Stage 38 is not validated until a fresh native + full ESP32-C6 build passes.
The deployment partition is now intended to be adapted to the current 16 MB
flash layout before flashing. The old Stage 35 1.31 MB application-partition
percentage is historical and no longer the target partition limit, but RAM,
binary size and final partition fit must still be recorded.


## Runtime LED topology

Side length, physical output and strip direction are commissioned as one
versioned profile.

    l<Enter>       status
    lreset<Enter>  default
    l230:18:0,160:19:0,230:20:0,160:21:0<Enter>

Per-side grammar:

    COUNT:GPIO:REV

Rules:

- COUNT is 1..230
- GPIO is one of 18/19/20/21
- every GPIO is used exactly once
- REV is 0/1
- brightness must be 0

Changing topology updates renderer, DDP expected frame bytes and ToF
perimeter sampling together. Old cached RGB/gains are invalidated safely.


## Disabled pixel mask

One pixel per logical segment can be forced permanently black at render time.

Serial:

    d<Enter>            status
    d-,12,-,0<Enter>    TOP none, RIGHT 12, BOTTOM none, LEFT 0
    dreset<Enter>       remove persisted mask

Web:

    LED commissioning -> Disabled pixel

Indexing is segment-relative and follows the commissioning START marker.

Ranges:

    TOP     0..229
    RIGHT   0..159
    BOTTOM  0..229
    LEFT    0..159

Use `-` or an empty web field for no disabled pixel.

The mask is applied after correction and logical-to-physical mapping, so the
selected pixel remains black in DISABLED, SHADOW, ACTIVE and commissioning
patterns. It does not remove a logical LED, shift neighbours or alter ToF
gain indexing.

## LED commissioning

Safe test brightness:

    1..64

Whole-frame patterns:

    i<Enter>   status
    i1         segment colors
    i2         START/MID/END direction markers
    i0         stop

Range probes:

    jside:SIDE:START:COUNT
    jgpio:GPIO:START:COUNT

SIDE is 0=TOP, 1=RIGHT, 2=BOTTOM, 3=LEFT.

Logical probes use the active topology, REV and disabled-pixel mask. Raw GPIO
probes bypass logical mapping and directly identify which physical strip is
connected to GPIO18/19/20/21.

Tests run for 15 seconds and then restore the newest HyperHDR frame or black.


## ToF commissioning

Web UI includes a normalized 8x8 zone matrix.

    POST /api/tof-debug  start
    POST /api/tof-debug  stop

Serial:

    z

toggles the same transient session.

A debug session lasts at most 60 seconds and changes ToF processing cadence
from about 12 seconds to about 1 second. It is observational, not persisted,
and is refused while correction is ACTIVE.

Each matrix cell reports:

    normalized row/column
    raw VL53L5CX zone index
    distance_mm
    target_status

The grid already applies current ROT/MIRROR, so its top row is TV TOP and its
left column is TV LEFT. Status 5 is full confidence; statuses 6 and 9 remain
usable at reduced plane-fit weight.

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


## Firmware identity

Immediate command:

    v

reports:

    firmware name/version
    development stage
    target MCU
    serial protocol version
    logical LED count
    DDP port
    persisted spatial/map schema versions

Current source identity:

    ambilight-c6 0.38.0-dev Stage 38
    serial protocol 2

Startup uses the same centralized FirmwareInfo constants instead of a handwritten stage banner.
