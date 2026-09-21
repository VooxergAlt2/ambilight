# WLED / Home Assistant compatibility facade

## Purpose

Stage 46 exposes a deliberately small WLED-compatible control and discovery
surface for Home Assistant without replacing the Ambilight firmware with WLED.

The architecture is:

    Home Assistant
        -> mDNS _wled._tcp
        -> HTTP/80 WLED-compatible JSON
        -> output power / brightness / RGB / manual effect

    HyperHDR
        -> DDP UDP/4048
        -> realtime RGB

    Ambilight Web UI
        -> HTTP/80 /api/*

The realtime renderer, DDP assembler, ToF correction, LED topology and PARLIO
pipeline remain native Ambilight components.

## Compatibility target

The facade is designed against:

- Home Assistant WLED integration using current python-wled 0.23.x semantics
- WLED API compatibility level 0.15.3

The facade reports:

    info.ver = 0.15.3
    info.ws  = -1

The websocket value disables the WLED websocket path in Home Assistant, so HA
uses its normal polling coordinator.

## Discovery

When Wi-Fi is connected and the HTTP server is running, firmware advertises:

    _http._tcp.local. port 80
    _wled._tcp.local. port 80

The WLED service includes:

    TXT mac=<lowercase 12-hex MAC>

The hostname is unique per controller:

    ambilight-c6-<last6mac>

Home Assistant can use the TXT MAC to deduplicate the device before its first
/json request.

Discovery stops on Wi-Fi loss or HTTP shutdown and retries after reconnect.

## Read endpoints

Supported:

    GET /json
    GET /json/state
    GET /json/info
    GET /json/eff
    GET /json/pal
    GET /presets.json

The combined /json response contains:

    state
    info
    effects
    palettes

The facade exposes exactly one full-length segment.

Effects:

    ["Ambilight", "Solid", "Rainbow", "Breathing"]

Palettes:

    ["Default"]

Presets wire response:

    {"0":{}}

This is an intentional compatibility sentinel. Current python-wled treats an
empty object as a failed presets fetch, but explicitly discards preset id 0
from its model. Home Assistant therefore sees no user preset while the initial
device update remains successful.

## Write endpoint

Supported:

    POST /json/state

The write must use:

    Content-Type: application/json

Accepted top-level WLED state fields include:

    on
    bri
    live
    v
    seg

Unknown valid JSON values are ignored rather than interpreted as Ambilight
configuration.

PUT is deliberately unsupported. Stage 46 is a Home Assistant compatibility
surface, not a generic WLED/Hyperk emulation layer.

## One-segment output model

The facade exposes one full-length RGB segment.

Segment brightness remains fixed at 255. The real global dimmer is master
state.bri:

    segment bri 255 * master bri / 255

This avoids double scaling.

The segment owns only manual visual controls:

    col -> RGB color
    fx  -> Ambilight / Solid / Rainbow / Breathing
    sx  -> animation speed
    ix  -> animation intensity

Master on/bri remain the sole physical power/brightness authority.

## Output semantics

Stage 46 separates output power from remembered brightness.

Examples:

    {"on":false}
        -> off, remember previous brightness

    {"on":true}
        -> on, restore remembered brightness

    {"bri":120}
        -> brightness 120

    {"bri":0}
        -> off without creating a visible on-at-zero state

If on=true is requested while remembered brightness is zero, firmware restores
the conservative default brightness.

The same WledCompat resolver is used for:

1. the synchronous HTTP state response predicted before mutation
2. the action later applied by the main runtime dispatcher

The HTTP response and actual state transition therefore share one semantic
implementation.

## HTTP acknowledgement ordering

The existing WebUiService safety rule remains intact:

    parse + validate request
      -> build predicted WLED state JSON
      -> send HTTP 200 completely
      -> close client
      -> release queued action to main
      -> mutate runtime / NVS

If the response cannot be delivered, the pending action is discarded.

This keeps WLED writes aligned with the existing web-control transaction model.

## Home Assistant option boundary

The intended Home Assistant shape is the default one-segment WLED entity with
"Keep master light" left disabled.

In this mode Home Assistant exposes one light with:

    on/off
    brightness
    RGB color
    effects

HA still performs its normal two-request one-segment turn-on sequence. Segment
on/bri are syntax-validated but ignored for physical power; the following
master request is authoritative. Segment RGB/effect fields are applied
immediately.

After upgrading from the brightness-only Stage 46 facade, reload the WLED
integration or restart Home Assistant. Supported color modes are initialized
when the HA entity is created and are not rebuilt merely because lc/seglc
changes on a later poll.

Enabling Home Assistant's optional "Keep master light" exposes separate master
and segment controls. That mode is not the intended UI for this facade.

## Home Assistant entity shape

The facade advertises one segment with:

    info.leds.maxseg = 1
    info.leds.lc     = 1
    info.leds.seglc  = [1]
    info.fxcount     = 4
    info.palcount    = 1
    segment bri      = 255

Capability 1 is WLED RGB. Home Assistant therefore exposes the RGB color
picker. The effect list maps directly to stable IDs:

    0 Ambilight
    1 Solid
    2 Rainbow
    3 Breathing

Selecting an RGB color without an explicit effect enters Solid automatically.
Selecting Ambilight releases manual ownership and restores the newest complete
DDP frame.

## Diagnostics ownership

DDP black-frame forensics remain source diagnostics rather than generic LED
output diagnostics.

Frames generated by:

    Home Assistant manual effects
    commissioning patterns
    control-path fallback blackouts

are rendered without feeding the DDP black-frame classifier.

Switching between Ambilight and a Home Assistant manual effect breaks only the
current black/recovery sequence. Historical DDP black counters remain intact.

The `frame_held` flag and WLED `info.live=DDP` are meaningful only while
the selected effect is Ambilight. DDP reception may continue in the background
during a manual effect so the newest complete frame is immediately available
when Ambilight is selected again.

## HyperHDR boundary

HyperHDR remains on:

    DDP UDP/4048

Stage 46 does not advertise _hyperk._tcp and does not implement the HyperHDR
Hyperk PUT control path.

This is deliberate. Current HyperHDR Hyperk configuration defaults include:

    brightnessMax      true
    brightnessMaxLevel 255

Its power-on request can therefore include bri=255. Stage 46 treats WLED bri
as the real global Ambilight brightness, so advertising Hyperk compatibility
could unexpectedly raise a controller configured at the conservative 32/255
level to full brightness.

Do not configure this firmware as a HyperHDR Hyperk device.

The normal WLED realtime UDP transport is also not implemented. For HyperHDR,
use the existing DDP device path on UDP/4048.

## Persistence

Output power and configured brightness are one atomic NVS record:

    output_state

Schema 1 stores:

    schemaVersion
    brightness
    enabled

Legacy Stage <=45 brightness and early Stage 46 output_on keys are migration
inputs only. They are removed after a successful output_state commit.

## Home Assistant auxiliary entities

The official Home Assistant WLED integration always forwards several WLED
platforms in addition to the light entity. Stage 46 cannot suppress those
platforms through device metadata.

Expected extra entities include diagnostics/config controls such as LED count,
IP address, restart, nightlight/sync, segment speed/intensity, reverse/freeze,
live override and firmware update.

They are outside the Stage 46 control contract. Unsupported state fields are
accepted as valid JSON but do not mutate Ambilight output. The restart endpoint
is not exposed.

The firmware-update entity is intentionally unable to install WLED firmware:
the facade reports:

    info.arch = ESP32-C6

Current python-wled's upgrade whitelist does not include ESP32-C6, so upgrade
is rejected client-side before any /update upload. A native regression test
locks this reported architecture to avoid accidentally widening that boundary.

For a clean HA dashboard, disable the unused WLED configuration/update
entities and keep the single RGB/effect light entity.

## Deliberately unsupported WLED features

The facade does not implement:

- full WLED renderer/effects engine beyond the small native manual set
- arbitrary segment creation or deletion
- presets or playlists
- WLED websocket push
- WLED realtime UDP / WARLS / E1.31
- WLED filesystem
- WLED OTA endpoints
- WLED configuration pages
- WLED realtime color ownership; manual RGB/effects are local firmware modes
- HyperHDR Hyperk control semantics

Ambilight-specific configuration remains under the native /api/* surface.

## Resource model

The WLED parser and serializer are fixed-buffer, heap-free C++ code.

WebUiProtocol allows a larger body only for WLED JSON writes. Existing native
Ambilight /api payloads keep their tighter bounded contract.

DDP polling remains ahead of the HTTP service in the main loop.

Manual effects do not replace DDP ingest. DDP continues to assemble the newest
complete frame while Solid/Rainbow/Breathing owns the physical output. Selecting
Ambilight hands output back to DDP immediately. Animated manual modes are
limited to about 30 FPS and are suspended while effective output brightness is
zero.

## Validation

Native contracts cover:

- WLED JSON state parsing
- nested/unknown JSON skipping
- brightness and on/off resolution
- segment preflight validation without power mutation
- RGB/effect/speed/intensity parsing
- current HA RGB capability shape
- manual visual state projection
- state JSON projection
- info JSON fields used by Home Assistant
- combined /json response body
- python-wled-safe invisible preset-0 sentinel
- response-buffer overflow failure
- HTTP GET/POST route and content-type rules
- explicit rejection of PUT and writes to combined /json
- legacy output-state migration
- atomic output-state write failure behavior

Full ESP32-C6 compilation and live Home Assistant discovery remain local/
hardware validation gates and must not be inferred from static source review.
