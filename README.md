# ESP32-C6 Ambilight Controller

Custom ESP32-C6 Ambilight endpoint for HyperHDR.

## Current development line

Stage 45 adds last-frame hold on top of the Stage 44.1 UI-hardening line. If no new complete DDP frame arrives, firmware no longer synthesizes an idle black frame: the physical LEDs keep the last successfully shown state until a newer complete frame or an explicit control action changes it.

The firmware stack now includes:

- Wi-Fi/DDP runtime transport
- one active DDP sender lease
- transport-gap last-frame hold with no automatic idle blackout
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
- lightweight HTTP/80 control UI with Home / LED / ToF / Diagnostics / System workflows
- browser-local Russian / English UI localization
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

    TOP     230 -> GPIO20 REV
    RIGHT   160 -> GPIO19 REV
    BOTTOM  230 -> GPIO21 REV
    LEFT    160 -> GPIO18 FWD
    total   780

Each side may use 1..230 active addresses. The active DDP payload is always:

    totalLedCount * 3 bytes

Topology changes perform an automatic controlled blackout, cancel any active LED commissioning test, apply the new mapping transactionally, restore the previous brightness, and start a fresh DDP sender/assembly epoch.

## DDP transport

Port:

    UDP/4048

The first structurally valid DDP sender acquires the stream lease.

A partial, dropped or missing frame is never converted into black output. DDP publishes only complete frames; if no newer complete frame exists, the renderer receives no RGB mutation and the LED hardware continues showing the last successfully transmitted state. A legitimate complete all-black frame from HyperHDR is still rendered normally.

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

## Web UI

The header includes a Русский / English selector. The choice is stored only in
that browser's `localStorage`, so different clients may use different
languages without changing controller NVS or API payloads.

When Wi-Fi is enabled:

    http://<controller-ip>/

The page is organized as:

- **Home**: correction mode, brightness, PC/DDP signal and a compact operational summary
- **LED**: TV-side topology, physical GPIO identification, direction/range tests and disabled-pixel mask
- **ToF**: normalized 8x8 matrix, mounting geometry, percentage-based distance/gain editor and calibration
- **Diagnostics**: compact DDP/ToF/heap/web runtime counters
- **System**: Wi-Fi provisioning and guarded factory reset

The browser shows distance/gain values as percentages and converts them losslessly to the existing Q12 runtime contract before POSTing.

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

Saved Wi-Fi passwords are never returned. The browser therefore requires either a newly entered password or an explicit «open network» choice before saving Wi-Fi credentials; an empty password field never silently replaces the stored WPA password.

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

Spatial edits are blocked in ACTIVE and while a 60-second calibration capture is active.

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

Each run runs the partition gate, embedded Web UI structural gate, native tests and firmware build, then writes logs under:

    .artifacts/validation/<timestamp>/

Stage 35 was validated locally on Windows with:

    147 native tests passed
    ESP32-C6 firmware build passed

Stage 39 adds a mandatory partition-layout gate before native/firmware checks. Stage 44 adds `tools/check_web_ui.py`; Stage 44.1 strengthens it with static HTML nesting checks, hardening-control contract checks and an exact integer proof of the 4097-value Q12↔percent round trip.
The target layout is now committed as:

    partitions/ambilight_16mb_ota.csv

It provides two 7 MiB application slots inside the 16 MiB flash image. The
local validation scripts run tools/check_partition.py first and fail if the
layout overlaps, loses required alignment, exceeds 16 MiB, or changes the
expected app-slot size.

Stage 39 was locally validated on Windows with the mandatory partition,
native and ESP32-C6 firmware gates all passing:

    partition_ok=1 (16 MiB flash, two 7 MiB app slots, 6 partitions)
    182 native test cases passed across 19 suites
    RAM 91636 / 327680 bytes (28.0%)
    PROGRAM 1269494 / 7340032 bytes (17.3%)
    firmware.bin 1306928 bytes; app-slot margin 6070538 bytes

The native result includes fault-injected RuntimeSettings/NVS reset semantics.


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
- topology changes perform their own controlled blackout; the user does not need to set brightness to 0

Changing topology updates renderer, DDP expected frame bytes and ToF
perimeter sampling together. Old cached RGB/gains are invalidated safely.
Any actual mapping change also clears all fixed 230-pixel PARLIO lane buffers,
so inactive physical addresses beyond a shortened/remapped side cannot retain
old RGB values.


## Disabled pixel mask

One physical pixel per strip can be forced permanently black at render time.

Serial/API payloads remain zero-based:

    d<Enter>            status
    d-,12,-,0<Enter>    TOP none, RIGHT physical offset 12,
                        BOTTOM none, LEFT physical offset 0
    dreset<Enter>       remove persisted mask

Web:

    LED commissioning -> Disabled pixel

The Web UI is 1-based: enter `1` for the first physical LED from the DATA
input, regardless of REV/FWD. Blank means no disabled pixel.

Default active web ranges are therefore:

    TOP     1..230
    RIGHT   1..160
    BOTTOM  1..230
    LEFT    1..160

Internally LedRenderer compares the stored physical offset against the
post-reversal `physicalIndex()`. Stage 45.3 bumps the pixel-mask NVS schema
from 1 to 2; old logical-offset masks are discarded at boot rather than being
reinterpreted.

The mask does not remove a logical LED, shift neighbours or alter DDP/ToF gain
indexing. Logical render/commissioning paths honor it; raw-GPIO tests bypass
logical mapping and are intended for physical lane identification.

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

Tests run for 120 seconds and then restore the newest HyperHDR frame or black.


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


## Gain-slew hardening

Stage 45.1 fixes a render-gain timebase bug that could bypass the configured slew limit after a long settled period. When a new non-fail-open gain profile arrives, the controller now rebases `lastUpdateUs_` to the target-change timestamp before reopening the slew state. An immediate `advance()` therefore performs no gain movement, and subsequent steps are bounded only by real elapsed time.

A native regression test covers: settled target -> long idle interval -> new target -> immediate advance -> 100 ms bounded step.

## Last-frame hold

Normal playback uses a hold-last policy for DDP transport gaps.

    complete DDP frame -> render newest frame
    partial/missing frame -> no RGB render, keep physical LED state
    later complete frame -> render immediately

There is no runtime timeout that turns input silence into an artificial black frame. This avoids a visible one-frame blackout when a short network/assembler gap crosses the former 1-second idle threshold.

This does **not** filter black video. A valid complete DDP frame containing black RGB is real source content and is rendered as black. Explicit brightness 0, topology safety blackout, commissioning behaviour and factory-reset blackout remain unchanged.

The Web UI reports `frame_held=true` once the most recent complete frame is older than 1 second. That threshold is telemetry only and does not modify LED output.

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

    ambilight-c6 0.45.3-dev Stage 45
    serial protocol 2

Startup uses the same centralized FirmwareInfo constants instead of a handwritten stage banner.
