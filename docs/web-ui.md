# Web UI

## Purpose

The Stage 45 web surface remains LAN-only and bounded. Stage 45 keeps the Stage 44.1 hardening and changes only the runtime policy for missing DDP complete frames: output now holds the last successfully shown frame instead of synthesizing an idle blackout.

It intentionally does not add:

- Arduino WebServer
- AsyncWebServer
- WebSocket
- external JavaScript/CSS frameworks
- CDN assets
- mDNS
- authentication database
- a dedicated FreeRTOS web task

The existing lwIP socket stack is reused directly.

The page remains one embedded HTML/CSS/JS document with no external assets. Stage 45.3 adds browser-local RU/EN localization while keeping the backend and HTTP payloads language-neutral. The UI still has five client-side views:

    Home
    LED
    ToF
    Diagnostics
    System

No route or socket architecture changes are required for navigation.

The header contains a Русский / English selector. The selected locale is stored only in browser localStorage under `ambilight.locale`; it is not written to controller NVS. On the first visit, Russian browser locales default to RU and other locales default to EN. Different clients may therefore use different languages at the same time.

This keeps the web layer small and prevents it from competing with realtime
DDP processing.

## Runtime path

    browser
      -> TCP/80
      -> WebUiService
      -> WebUiProtocol
      -> queued WebUiActionEvent
      -> existing runtime parser/domain object
      -> existing apply/reset handler
      -> NVS / ToF / renderer / Wi-Fi

The web UI does not emulate serial input.

Serial and HTTP converge on the same runtime validation and apply functions.

## Scheduling

The web service runs from the main loop after the DDP backlog gate.

DDP therefore retains priority.

The service is deliberately bounded:

    one active HTTP client
    request buffer       1536 B
    request body         <= 127 B
    receive per loop     <= 512 B
    send per loop        <= 1024 B
    client idle timeout  2 s
    keep-alive           disabled

Large page responses are sent over multiple loop iterations.

No blocking send loop is used.

## HTTP routes

Read-only:

    GET /
    GET /api/status

Runtime actions:

    POST /api/brightness
    POST /api/correction
    POST /api/test
    POST /api/led-map
    POST /api/pixel-mask
    POST /api/spatial
    POST /api/curve
    POST /api/wifi
    POST /api/calibration
    POST /api/shadow-probe
    POST /api/tof-debug
    POST /api/factory

POST bodies use the same compact text payloads as existing runtime commands.

Examples:

    /api/brightness   32
    /api/correction   0 | 1 | 2
    /api/test         0 | 1 | 2
    /api/test         side:2:100:10
    /api/test         gpio:20:0:37
    /api/led-map      230:18:0,160:19:0,230:20:0,160:21:0
    /api/led-map      reset
    /api/pixel-mask   -,12,-,0
    /api/pixel-mask   reset
    /api/spatial      1437.5,1000,0,0,0,0,0,10
    /api/spatial      reset
    /api/curve        50:2048,500:3072,4000:4096
    /api/curve        reset
    /api/wifi         SSID|PASSWORD
    /api/wifi         clear
    /api/calibration  start
    /api/shadow-probe start
    /api/tof-debug    start | stop
    /api/factory      reset

## Action acknowledgement

A state-changing request is not released to main immediately.

Sequence:

1. HTTP request is fully parsed.
2. firmware queues an action id
3. 202 Accepted is sent to the browser
4. TCP client is closed
5. only then is WebUiActionEvent released to main
6. main executes the normal runtime action
7. /api/status reports the action id and actual result

The browser correlates its exact queued id. If a later web action from another client has already replaced the single retained action result, the original browser releases its pending lock without clearing form drafts and reports that the result was superseded. A bounded timeout also prevents a reboot/lost-result path from wedging the UI forever.

This is important for:

- Wi-Fi credential changes
- clearing Wi-Fi credentials
- factory reset

Those actions may tear down the network or reboot the MCU.

## Safety contracts

The web UI does not weaken existing guards.

LED topology:

    COUNT 1..230
    GPIO one of 18/19/20/21, unique
    REV 0/1
    active LED commissioning is cancelled before a topology transaction
    controller performs the safety blackout and restores brightness automatically

Factory reset:

    brightness = 0

Commissioning patterns:

    brightness = 1..64

Spatial profile edits:

    not ACTIVE
    not while calibration capture is active

Gain curve edits:

    not ACTIVE

Shadow probe:

    SHADOW only

ToF live debug:

    DISABLED or SHADOW only
    transient, max 60 s

Invalid web payloads are passed through the same existing typed runtime
parsers before a setting is applied.

## LED topology commissioning

The LED view first visualizes the active TV perimeter and then edits all four logical sides as one topology:

    COUNT | GPIO | REV

Rows are always:

    TOP
    RIGHT
    BOTTOM
    LEFT

The UI also displays the resulting total logical LEDs and DDP RGB byte count. The TV diagram shows each side's active count, GPIO and direction arrow.
Topology Apply/Reset no longer requires the user to set brightness to zero:
the backend owns the controlled blackout transaction and remains authoritative.

Two probe modes are available:

Logical side range:

    side:SIDE:START:COUNT

This passes through the active topology, reversal and disabled-pixel mask.

Raw GPIO range:

    gpio:GPIO:START:COUNT

This directly addresses the physical PARLIO lane for GPIO18/19/20/21 and
bypasses logical mapping. It is intended for identifying which installed
strip is physically connected to which output.

Both probe types require brightness 1..64 and stop automatically after the
normal commissioning timeout.

## ToF live commissioning

The ToF commissioning section contains a normalized 8x8 live matrix.

Starting:

    POST /api/tof-debug
    start

temporarily changes processed ToF cadence from about 12 seconds to about
1 second. The session stops automatically after 60 seconds or when ACTIVE
correction is entered.

The matrix is normalized by the current TofSpatialProfile ROT/MIRROR before it
is sent to the browser. Each cell still includes its original raw VL53L5CX
zone index.

A cell shows:

    distance_mm
    raw index
    target_status

Selecting a cell expands:

    normalized row/column
    raw index
    distance
    status classification
    active ROT/MIRROR

Status 5 is full-confidence. Statuses 6 and 9 are usable at reduced plane-fit
weight. Rejected statuses stay visible rather than being hidden.

The ToF view keeps spatial controls beside the live matrix. Rotation is shown as 0°/90°/180°/270° and mirror state uses user-facing labels while the backend payload remains ROT=0..3 and MIRROR=0/1. Edits remain blocked in ACTIVE.

The gain editor accepts `distance_mm:percent` values. JavaScript validates 2..8 monotonic points and converts percentages to the unchanged Q12 runtime payload. Status rendering uses enough decimal precision that every Q12 value 0..4096 round-trips through the percentage editor without loss.

## Disabled pixel mask

The LED commissioning section exposes one optional disabled **physical** LED
for each strip/lane.

The Web UI is 1-based for humans:

    1 = first physical LED from the controller / DATA input
    N = last active physical LED on that strip
    blank = no disabled pixel

REV/FWD does not change this number. Reversal maps logical perimeter positions
onto the physical strip, while the mask is applied after that mapping.

The compact HTTP/serial payload remains zero-based:

    TOP,RIGHT,BOTTOM,LEFT

with `-` for none. For example:

    -,12,-,0

means no TOP mask, physical offset 12 on RIGHT, no BOTTOM mask, and physical
offset 0 (the first LED on the wire) on LEFT.

Bounds follow the active runtime length of each side. If a topology change
shortens a side below a stored physical offset, that mask entry is
automatically cleared.

Stage 45.3 changes the persisted meaning from logical offset to physical strip
offset and therefore bumps `LedPixelMaskProfile::kSchemaVersion` to 2.
Schema-1 masks are invalidated on boot instead of being silently reinterpreted.

The mask is applied by LedRenderer after logical-to-physical mapping and after
correction selection. It therefore remains black in DISABLED, SHADOW and
ACTIVE render paths without removing a logical LED or changing DDP/ToF
indexing. Raw-GPIO commissioning intentionally bypasses logical rendering and
is not a mask verification path; logical-side tests do exercise the mask.

## Calibration

The web UI can start the existing 60-second observational capture.

The latest CalibrationCaptureSummary is retained in RAM and returned by
/api/status so the result remains visible in the browser after capture
completion. Successful spatial-profile or LED-topology changes invalidate the old summary, and both changes are refused while capture is active so one result cannot mix multiple geometries.

It is not persisted to NVS.

Starting a second capture while one is already active is refused.

## Status surface

The compact status endpoint includes:

- firmware version/stage
- correction mode and brightness
- Wi-Fi state, SSID, IP and RSSI
- DDP state, active sender and age of the most recent complete frame
- frame-hold state when input silence is being bridged by the last successfully shown frame
- ToF state/age/zones/median
- plane yaw/pitch/accepted zones
- perimeter min/max distance and fail-open state
- spatial profile
- gain curve
- runtime LED topology including COUNT/GPIO/REV
- per-segment disabled-pixel mask
- normalized 8x8 ToF debug grid and raw zone ids
- ToF debug session state/remaining time
- commissioning state, active range target and runtime maximum test brightness
- calibration state/result
- shadow probe state
- latest web action result
- heap diagnostics
- web request/action/error counters

Saved Wi-Fi password is never returned. Because the password field is intentionally blank on every page load, saving Wi-Fi requires either a newly entered password or an explicit «open network» checkbox. Blank password input by itself is never interpreted as permission to overwrite a stored protected-network password.


## Localization

Static labels, validation messages and live operational summaries are available
in Russian and English. Localization is a browser concern only: route names,
JSON fields, runtime parsers and persisted controller settings do not depend on
the selected language.

## Operational-state hardening

Stage 44.1 treats controller state as authoritative rather than trusting the last click. The brightness slider is locked while an action is in flight and rolls back to controller state after an immediate/confirmed failure. Temporary status-poll failures clear after communication recovers. ToF summary cards distinguish startup/error/fail-open states, and an old 8x8 snapshot is visually marked as stale whenever the sensor is not actively ranging.

## DDP transport-gap hold

The renderer only receives new RGB when DDP publishes a complete frame.

Stage 45 removes the former 1-second idle-blackout mutation. If an input frame is partial, dropped, delayed or otherwise never completes, no replacement black frame is generated. The physical LED driver therefore keeps the previous successfully shown state until the next complete DDP frame arrives.

The `frame_held` status flag becomes true after the last complete frame is older than 1 second. It is diagnostic only; it does not trigger a render and it does not impose a later blackout timeout.

A complete all-black DDP frame remains authoritative source data and is rendered normally. Explicit output controls, topology safety blackout, commissioning and factory recovery are unaffected.

## LED colour order scope

The minimal UI does not expose a separate colour-order setting.

The configured LiteLED strip type is:

    LED_STRIP_WS2812

LiteLED defines that strip type with the standard WS2812/WS2812B GRB wire
order while application colours remain normal RGB values.

If hardware commissioning proves that the actual strip uses a non-standard
order, LiteLEDpioLane supports runtime setOrder(). That hardware-specific
override can be added later without changing logical RGB, DDP or ToF
semantics.

## Developer diagnostics remain serial-only

Low-level dumps are intentionally not duplicated in the minimal UI:

    t  raw 8x8 ToF map
    g  legacy band diagnostics
    p  detailed plane diagnostics
    k  legacy gains
    s  full spatial gain diagnostics
    r  detailed render diagnostics

These remain available through serial for engineering investigation.

## Browser security boundary

This is a trusted-LAN control surface, not an internet-facing management
server.

There is no user/password authentication.

State-changing requests require:

    X-Ambilight-Control: 1

The firmware sends no CORS permission and does not implement OPTIONS.

This prevents ordinary cross-origin browser forms/fetches from issuing simple
control requests without a preflight failure.

It is not a substitute for network isolation.

Do not expose TCP/80 to the public internet.

## Wi-Fi lifecycle

The listener exists only while Wi-Fi is enabled.

Startup:

    Wi-Fi enabled
      -> DDP socket
      -> HTTP/80 listener

Clearing credentials with no compile-time fallback:

    web stop
    Wi-Fi off
    DDP stop

If compile-time fallback exists, DDP and the web listener continue using the
fallback network.

## Validation

`tools/check_web_ui.py` performs a dependency-free structural check of the embedded page. It rejects duplicate static DOM IDs, missing page/navigation pairs, mis-nested static HTML, removal of required hardening controls/actions, and reintroduction of stale hardcoded safety guards. It also proves all 4097 Q12 values survive the user-facing 3-decimal percent editor and return to the original Q12 value. Both `tools/validate.ps1` and `tools/validate.sh` run this check alongside the partition, native and firmware gates.

WebUiProtocol is pure C++ and participates in the native PlatformIO gate.

Native contracts cover:

- GET index/status
- every POST action route
- required control header
- incomplete request/body
- exact 127-byte payload boundary
- oversized body rejection
- conflicting Content-Length rejection
- chunked transfer rejection
- unknown route/method
- GET body rejection
- malformed Content-Length
- binary/control body rejection

The lwIP socket service itself is compiled only by the full ESP32-C6 firmware
build.

Stage 45 must not be treated as validated until the full local validation passes:

    tools/check_partition.py
    tools/check_web_ui.py
    pio test -e native
    pio run -e esp32-c6-devkitc-1

The final firmware build must use/verify the intended 16 MB flash partition
layout before hardware flashing. Record RAM, binary size and actual partition
fit; the historical Stage 35 1.31 MB app partition is no longer the target
limit.
