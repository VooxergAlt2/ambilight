# Architecture

## Current stage

Stage 39 is the pre-flash hardening line. Stage 38 remains the feature baseline; Stage 39 hardens topology transitions, physical PARLIO buffer cleanup, reset durability and the 16 MiB deployment layout before hardware commissioning.

The active firmware now combines:

- realtime Wi-Fi/DDP RGB transport
- one active DDP sender lease
- checked UDP socket receive-buffer tuning
- fixed 920-LED memory capacity with runtime active logical count
- four synchronized ESP32-C6 PARLIO outputs
- runtime LED side length / GPIO / reversal topology
- slow VL53L5CX wall-plane geometry
- exact active-perimeter distance/gain field
- runtime ToF spatial and photometric calibration
- DISABLED / SHADOW / ACTIVE correction modes
- atomic runtime output power + remembered brightness
- commissioning patterns
- guarded NVS factory recovery
- generic non-RGB render-state scheduling
- typed pure-C++ serial command framing
- typed pure-C++ runtime payload parsing
- centralized firmware identity/version diagnostics
- minimal lwIP HTTP/80 commissioning/control UI
- WLED-compatible Home Assistant control facade
- mDNS WLED discovery
- persisted per-segment disabled-pixel mask
- logical-side and raw-GPIO LED range probes
- transient normalized 8x8 ToF live-debug mode

USB/AWA work remains preserved separately in:

    stage/07-usb-awa

## RGB transport path

    HyperHDR
      -> Wi-Fi
      -> UDP/4048
      -> DdpSenderGate
      -> DdpAssembler
      -> FrameMailbox
      -> cached RgbFrame
      -> RenderScheduler
      -> RenderGainController
      -> LedRenderer
      -> runtime LedMappingProfile
      -> LedEngine
      -> PARLIO x4

## Web control path

Native browser control:

    browser
      -> TCP/80
      -> WebUiService
      -> WebUiProtocol
      -> acknowledged queued WebUiActionEvent
      -> main runtime action dispatcher
      -> existing typed parser/domain object
      -> existing apply/reset handler

Compatibility control:

    Home Assistant
      -> mDNS _wled._tcp discovery
      -> TCP/80
      -> WebUiProtocol
      -> WledCompat parser / serializer / state resolver
      -> acknowledged queued WledState action
      -> shared output-state apply path

WledCompat owns the compatibility schema. WebUiService only supplies HTTP
transport/envelopes and snapshot adaptation.

Realtime RGB remains independent:

    HyperHDR
      -> DDP UDP/4048

No WLED realtime UDP or HyperHDR Hyperk control transport is implemented.
This separation prevents an external WLED-style power-on request from
overriding the controller's configured global brightness.

The native web layer does not emulate serial bytes and does not duplicate
domain validation.

The listener is deliberately small:

    one active client
    no keep-alive
    no WebSocket
    1536 B request buffer
    <=127 B native /api body
    <=511 B WLED JSON body
    <=512 B recv per loop
    <=1024 B send per loop
    2 s idle timeout

DDP polling and its backlog gate run before the web service in the main loop.

Native actions are released to main only after their HTTP acknowledgement has
been sent and the client socket closed.

WLED writes return a predicted WLED state body before mutation, then release
the already validated action after the response closes. Prediction and actual
application use the same WledCompat state resolver.

Developer-only raw ToF/render dumps remain serial-only.

## DDP sender ownership

Sender identity:

    IPv4 + UDP source port

The first structurally valid DDP packet acquires a one-second lease.

While leased:

    owner packets   -> assembler
    foreign packets -> drop

On timeout:

- ownership clears
- partial assembly clears
- sequence history clears
- another sender may acquire a new stream epoch

Rejected sender traffic cannot by itself trigger RGB render deferral.

## UDP socket

Firmware requests:

    SO_RCVBUF = 32768

and queries the actual value back with:

    getsockopt(SO_RCVBUF)

Socket-option failures are observable but non-fatal.

## Runtime LED topology

Memory capacity is fixed and heap-free:

    4 PARLIO lanes x 230 = 920 LED capacity

LedMappingProfile schema 3 is the authoritative topology. For each logical TV
side it stores:

- active logical length, 1..230
- physical PARLIO lane, surfaced to users as GPIO18/19/20/21
- FWD/REV direction

The four active segments remain contiguous in logical order:

    TOP -> RIGHT -> BOTTOM -> LEFT

Their starts are recomputed from the configured lengths. Therefore:

    active DDP bytes = totalLedCount * 3

There are no logical holes and inactive capacity is never part of the DDP
frame.

Every physical GPIO must be assigned exactly once. ToF gains are indexed in
logical screen space before physical reversal, so wiring cannot reverse the
wall model.

A topology apply owns its safety blackout and does not require the operator to
set brightness to zero manually. It acts as a coordinated transaction:

1. cancel active commissioning and enter controlled physical blackout
2. queue the new topology to ToF
3. reconfigure DDP expected frame bytes and reset sender/assembly epoch
4. switch renderer mapping
5. clear fixed physical lane buffers when mapping actually changes
6. prepare a sanitized disabled-pixel mask without mutating persistence yet
7. commit/reset the runtime topology in NVS
8. persist any required mask sanitation only after topology commit
9. publish a black frame with the new pixelCount
10. reset gain-controller state and wait fail-open for fresh ToF projection
11. restore configured effective output state

## ToF path

    VL53L5CX 8x8
      -> perpendicular distance_mm = Z
      -> runtime rotation/mirror normalization
      -> X/Y reconstruction
      -> robust wall-plane fit
      -> PlaneChangeGate
      -> active runtime logical LED positions
      -> +Z intersection with wall plane
      -> active throw-distance field
      -> runtime DistanceGainCurve
      -> PerimeterGainSnapshot.logicalGainQ12[capacity]
      -> TofRenderGainBridge
      -> RenderGainContext.logicalGainQ12[capacity]

Only indices below topology.totalLedCount() are active. Capacity arrays are
fixed at 920 entries to avoid heap allocation during commissioning.

## Wall model

    z_wall = intercept + slope_x*x + slope_y*y

For one LED:

    distance_i = z_wall(x_i, y_i) - z_led_i

This is the throw distance along screen/sensor +Z to the wall spot.

It is intentionally not the shortest orthogonal distance to a tilted wall.

## Runtime spatial profile

The authoritative mounting configuration is TofSpatialProfile.

Stored fields:

- LED perimeter width/height
- sensor X/Y offset
- LED plane Z offset
- sensor rotation
- sensor mirror
- plane deadband

The old compile-time ScreenGeometry configuration has been removed.

## Plane deadband

The candidate and last applied planes are compared at screen corners.

Because their difference is linear, the maximum absolute Z difference over a rectangle occurs at a corner.

Default threshold:

    10 mm

Below threshold:

- refresh source freshness
- keep existing active gain field

At/above threshold:

- accept new plane
- rebuild all active distances/gains

Skipped candidates do not move the accepted reference, so slow motion accumulates.

## Rate domains

DDP/RGB:

    realtime

VL53L5CX internal ranging:

    1 Hz

Pose transfer/processing:

    normal: about every 12 s
    ToF debug: about every 1 s for at most 60 s

Main ToF target polling:

    1 Hz

Render state-only rerenders:

    up to about 60 Hz

Fresh RGB frames bypass the state-only limiter.

## Render state

Non-RGB state-dirty sources currently include:

- ToF gain target/slew
- correction mode
- global brightness
- LED mapping changes
- disabled-pixel mask changes

RenderScheduler treats these generically as output state rather than incorrectly calling all of them gain changes.

## Correction modes

    DISABLED
        original HyperHDR RGB
        gain pipeline ignored for output

    SHADOW
        calculate full correction candidate
        original HyperHDR RGB to LEDs

    ACTIVE
        calculate full correction candidate
        corrected candidate to LEDs

Fail-open always resolves effective gain to unity.

Therefore invalid/stale ToF in ACTIVE returns physical output to original RGB.

## Runtime calibration

DistanceGainCurve is persisted in NVS and supports 2..8 monotonic points.

Curve edits are refused in ACTIVE.

A curve change:

1. fail-opens old gain snapshots
2. updates the ToF task
3. resets plane-change reference
4. forces the next valid pose to rebuild all active gains

The compiled default curve remains neutral until real photometric commissioning.

## Output brightness

Global LiteLED brightness:

    0..255

Default:

    32

It is a final multiplier independent from ToF gain.

## Disabled pixel mask

LedPixelMaskProfile stores one optional segment-relative offset for each
logical segment.

The sentinel value means no disabled pixel.

The mask is evaluated by LedRenderer after correction output is selected and
after SegmentMapper has resolved the physical lane/index. The decision uses
the logical segmentOffset, so runtime lane changes or FWD/REV mapping do not
change which screen-space LED is disabled.

A masked pixel is written as black in every correction mode and in
commissioning patterns.

The mask does not alter:

- logical segment lengths
- DDP payload size
- logical LED indices
- ToF distance/gain arrays
- physical indices of neighbouring LEDs

## ToF commissioning/debug

ToF debug is transient and is refused in ACTIVE correction mode.

Normal operation keeps the slow pose cadence. An explicit debug session:

    web: POST /api/tof-debug start
    serial: z

temporarily enables roughly 1-second processed frames for up to 60 seconds.
The sensor's persisted spatial profile is not modified by entering debug.

The web status surface exposes a normalized 8x8 grid. Each cell contains:

- perpendicular distance_mm
- target_status
- original raw VL53L5CX zone index

Normalization uses the active TofSpatialProfile rotation/mirror transform.
This makes the UI matrix screen-relative while still exposing raw zone ids
for mounting/orientation diagnosis.

Status 5 is full plane-fit weight. Statuses 6 and 9 are usable at 0.5 weight.
Other statuses remain visible for diagnosis but are rejected from normal
processor/plane input.

## Commissioning patterns

Temporary LED commissioning can verify:

- physical GPIO/strip identity with a raw lane probe
- logical TV-side assignment
- screen-space strip direction
- exact logical or physical address ranges

They run only at brightness 1..64 and bypass ToF correction for the test frame.

DDP continues receiving in the background.

After the test, the newest DDP frame is restored or output is blacked if no RGB source exists.

## Persistent configuration

NVS namespace:

    ambilight

Runtime configuration includes:

- correction mode
- atomic output_state (power + remembered brightness)
- Wi-Fi credentials
- ToF gain curve
- ToF spatial profile
- LED topology profile (LedMappingProfile schema 3)
- disabled-pixel mask profile

Versioned blobs use a data-first/version-last commit pattern.

Invalid persisted profiles fail back to firmware defaults.

## Recovery

Factory recovery requires:

    effective physical brightness = 0
    freset

The NVS namespace is cleared before in-memory defaults are changed.

If durable clear fails, firmware does not restart and live settings remain unchanged.

## Independence of failures

ToF failure does not restart or block DDP.

Wi-Fi reconnect does not reset ToF.

Invalid/stale ToF only fail-opens correction.

DDP sender handoff resets only transport assembly epoch.

Optional socket tuning failure does not disable DDP.

## Validation strategy

Software behavior is locked with deterministic native tests where hardware APIs are not required.

GitHub Actions remain manual because repository Actions quota is exhausted.

Stage 39 passed its local Windows validation gates before hardware
commissioning:

1. tools/check_partition.py: `partition_ok=1`
2. 19 native suites: 182 test cases passed
3. full ESP32-C6 firmware build: RAM 91636 / 327680 bytes (28.0%), PROGRAM
   1269494 / 7340032 bytes (17.3%)

Deployment is pinned to partitions/ambilight_16mb_ota.csv:

    flash      16 MiB
    app0       7 MiB
    app1       7 MiB
    storage    0x1E0000 bytes
    coredump   64 KiB

PlatformIO is configured with a 7 MiB maximum application image so partition
fit is checked by the normal firmware build. The validated `firmware.bin` is
1306928 bytes, leaving 6070538 bytes of app-slot margin.

Physical commissioning remains a later stage for:

- actual GPIO/strip verification
- sensor orientation and optical bias
- exact X/Y/Z mounting offsets
- real noise/deadband tuning
- real photometric distance/gain points


## Serial runtime control path

    USB serial bytes
      -> SerialCommandParser
      -> SerialCommandEvent
      -> main dispatch
      -> subsystem semantic handler

The parser is pure C++ and participates in native tests.

Framing errors discard the rest of the damaged line before returning to idle parsing. Subsystem validation stays outside the parser.


## Runtime payload path

For typed configuration commands:

    SerialCommandEvent payload
      -> RuntimePayloadParser
      -> domain object
      -> runtime apply/persist handler

The parser directly produces existing validated domain types instead of parallel DTO layers.


## Firmware identity

Firmware identity is centralized in config/FirmwareInfo.h.

The same constants feed:

- startup banner
- v serial command
- STATCFG

This prevents commissioning logs from depending on a manually maintained banner string.
