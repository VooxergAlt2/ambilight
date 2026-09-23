# Architecture

## Current architecture

The current firmware combines the Home Assistant RGB/effects compatibility
surface, physical pixel-mask hardening, the fail-open ToF/topology fixes, and
the delayed Wi-Fi recovery AP on one maintained `main` line.

The major concerns remain intentionally orthogonal:

- disabled-pixel masking is enforced as a physical lane invariant immediately
  before every PARLIO encode;
- Home Assistant may temporarily own visible RGB/effects, while HyperHDR DDP
  continues receiving in the background and regains output through the
  `Ambilight` effect;
- Wi-Fi recovery may expose a fallback AP after a 60-second STA outage without
  becoming part of the realtime render path.

The active firmware now combines:

- realtime Wi-Fi/DDP RGB transport
- one active DDP sender lease
- checked UDP socket receive-buffer tuning
- dynamic RGB/DDP/per-pixel storage sized from the active runtime topology
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

There is no separate 230-per-lane or 920-total policy limit. The four
`uint16_t` side-length fields imply a representable aggregate of up to 262,140 LEDs. `LedMappingProfile`
stores each of the four side lengths as `uint16_t`; aggregate logical offsets
and frame sizes use wider runtime types. The practical limit is available
ESP32-C6 memory plus physical PARLIO/LED timing.

Hardware validation has been performed up to 230 LEDs on one output. Larger
configurations are software-supported but experimentally unvalidated on a real
strip.

Runtime buffers follow the topology rather than the historical maximum:

- main RGB frames allocate `totalLedCount` pixels;
- DDP staging/coverage allocates `totalLedCount * 3` bytes plus coverage bits;
- PARLIO allocates every physical lane to the current longest side;
- ToF/render gain fields allocate one Q12 value per active logical LED.

The four logical segments remain contiguous:

    TOP -> RIGHT -> BOTTOM -> LEFT

A topology update is transactional: runtime RGB memory is preflighted, output
is blacked out, PARLIO/DDP/renderer/ToF are reconfigured, NVS is committed, and
brightness is restored. Allocation or subsystem failure rejects/rolls back the
change. Optional ToF gain allocation failure is fail-open and does not disable
normal DDP output.

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
      -> PerimeterGainSnapshot.logicalGainQ12[active topology]
      -> TofRenderGainBridge
      -> RenderGainContext.logicalGainQ12[active topology]

Gain arrays are sized to `topology.totalLedCount()` on the slow control/ToF
path. Realtime rendering reuses that storage and performs no allocation.

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

LedPixelMaskProfile stores one optional **physical strip offset** for each TV
side. The value is counted from that strip's DATA input and is independent of
FWD/REV logical screen direction.

LedRenderer projects those side values through the current LedMappingProfile
into a LedPhysicalPixelMask keyed by PARLIO lane.

Enforcement happens in LedEngine::show(), after any producer has written lane
buffers but immediately before physical encoding. The masked address is
therefore black for every output producer:

- normal DDP rendering
- ACTIVE ToF rendering
- logical commissioning
- raw physical/GPIO commissioning
- Home Assistant manual colors/effects

This also removes the mask branch from the per-pixel renderer hot loop.

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

## Output ownership

Physical LED output has one explicit priority order:

    commissioning diagnostic
        > Home Assistant manual effect
        > Ambilight / DDP

DDP reception continues while a manual effect owns the LEDs, but does not
mutate physical output. Selecting the WLED-compatible `Ambilight` effect
releases manual ownership and immediately renders the newest complete DDP
frame.

The disabled-pixel mask is enforced below all three owners in
`LedEngine::show()`, immediately before physical encoding.

## Commissioning patterns

Temporary LED commissioning can verify:

- physical GPIO/strip identity with a raw lane probe
- logical TV-side assignment
- screen-space strip direction
- exact logical or physical address ranges

They run only at brightness 1..64 and bypass ToF correction for the test frame.

DDP continues receiving in the background.

After the test, the previous output owner is restored: a Home Assistant manual
effect is rerendered immediately, otherwise the newest complete DDP frame is
restored, or output is blacked if Ambilight mode has no RGB source.

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

The local validation harness is the release gate. The manual GitHub Actions
workflow mirrors the same four gates on the maintained `main` line when hosted
runners are available:

1. `tools/check_partition.py`
2. `tools/check_web_ui.py`
3. `pio test -e native`
4. `pio run -e esp32-c6-devkitc-1`

The public-preparation baseline passed with 272 native tests, RAM usage
70460 / 327680 bytes (21.5%), and an application image of
1342096 / 7340032 bytes (18.3%).

Deployment is pinned to partitions/ambilight_16mb_ota.csv:

    flash      16 MiB
    app0       7 MiB
    app1       7 MiB
    storage    0x1E0000 bytes
    coredump   64 KiB

PlatformIO is configured with a 7 MiB maximum application image so partition
fit is checked by the normal firmware build. Exact image size is expected to
move with normal development; the partition gate and PlatformIO limit are
authoritative.

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
