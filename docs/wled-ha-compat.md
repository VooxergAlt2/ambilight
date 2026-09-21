# WLED / Home Assistant compatibility facade

## Purpose

Stage 46 exposes a deliberately small WLED-compatible control and discovery
surface for Home Assistant without replacing the Ambilight firmware with WLED.

The architecture is:

    Home Assistant
        -> mDNS _wled._tcp
        -> HTTP/80 WLED-compatible JSON
        -> output power / brightness

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

    ["Solid"]

Palettes:

    ["Default"]

Presets:

    {}

The empty presets endpoint is intentional. Current python-wled may request
/presets.json during the first device update based on info.fs metadata.

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

## One-segment brightness model

The facade exposes one segment with segment brightness fixed to 255. The real
dimmer is master state.bri.

This matches current Home Assistant behavior for a single-segment WLED device:
HA sends the segment brightness as 255 and then sends the requested brightness
through the master state.

Reported entity brightness is therefore:

    segment bri 255 * master bri / 255

There is no double scaling.

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

## Home Assistant entity shape

The facade advertises one segment with:

    info.leds.maxseg = 1
    info.leds.lc     = 2
    info.leds.seglc  = [2]
    segment bri      = 255

In the current Home Assistant WLED integration, capability 2
(WHITE_CHANNEL) maps to ColorMode.BRIGHTNESS. The controller therefore
appears as a dimmable light without exposing an RGB picker that would conflict
with realtime DDP colors.

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

## Deliberately unsupported WLED features

The facade does not implement:

- WLED renderer/effects engine
- arbitrary segment creation or deletion
- presets or playlists
- WLED websocket push
- WLED realtime UDP / WARLS / E1.31
- WLED filesystem
- WLED OTA endpoints
- WLED configuration pages
- WLED color ownership
- HyperHDR Hyperk control semantics

Ambilight-specific configuration remains under the native /api/* surface.

## Resource model

The WLED parser and serializer are fixed-buffer, heap-free C++ code.

WebUiProtocol allows a larger body only for WLED JSON writes. Existing native
Ambilight /api payloads keep their tighter bounded contract.

DDP polling remains ahead of the HTTP service in the main loop.

## Validation

Native contracts cover:

- WLED JSON state parsing
- nested/unknown JSON skipping
- brightness and on/off resolution
- segment on/off behavior
- current HA brightness capability shape
- state JSON projection
- info JSON fields used by Home Assistant
- combined /json response body
- empty presets response
- response-buffer overflow failure
- HTTP GET/POST route and content-type rules
- explicit rejection of PUT and writes to combined /json
- legacy output-state migration
- atomic output-state write failure behavior

Full ESP32-C6 compilation and live Home Assistant discovery remain local/
hardware validation gates and must not be inferred from static source review.
