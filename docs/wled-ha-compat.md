# WLED / Home Assistant compatibility facade

## Purpose

Stage 46 exposes a deliberately small WLED-compatible control and discovery
surface without replacing the Ambilight firmware with WLED.

The architecture is:

    Home Assistant
        -> mDNS _wled._tcp
        -> HTTP/80 WLED-compatible JSON
        -> output power / brightness

    HyperHDR Hyperk driver
        -> mDNS _hyperk._tcp
        -> HTTP/80 WLED-compatible JSON for control
        -> DDP UDP/4048 for realtime RGB

    Ambilight Web UI
        -> HTTP/80 /api/*

The realtime renderer, DDP assembler, ToF correction, LED topology and PARLIO
pipeline remain native Ambilight components.

## Compatibility target

The facade is designed against:

- Home Assistant WLED integration using python-wled 0.23.x
- WLED API compatibility level 0.15.3
- HyperHDR DriverNetHyperk, which inherits the DDP driver and uses the WLED
  shaped HTTP API only for state/control

The facade reports:

    info.ver = 0.15.3
    info.ws  = -1

The websocket value disables the WLED websocket path in Home Assistant, so HA
uses its normal polling coordinator.

## Discovery

When Wi-Fi is connected and the HTTP server is running, firmware advertises:

    _http._tcp.local.   port 80
    _wled._tcp.local.   port 80
    _hyperk._tcp.local. port 80

The WLED service includes:

    TXT mac=<lowercase 12-hex MAC>

The hostname is unique per controller:

    ambilight-c6-<last6mac>

The _wled service is the Home Assistant discovery contract.

The _hyperk service is retained for HyperHDR Hyperk discovery. Failure to
register the optional _hyperk alias does not tear down a valid _wled service.

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
    PUT  /json/state

Accepted top-level WLED state fields include:

    on
    bri
    live
    v
    seg

Unknown valid JSON values are ignored rather than interpreted as Ambilight
configuration.

Only the single exposed segment is relevant. Segment on/off is accepted.
Segment brightness is validated but the exposed segment brightness remains
255, because the actual dimmer is the WLED master brightness.

This avoids double scaling in Home Assistant:

    HA effective brightness =
        segment bri 255 * master bri / 255

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

If on=true is requested while remembered brightness is zero, the firmware
restores the conservative default brightness.

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

This prevents a state-changing request from tearing down its own TCP response
and keeps WLED writes aligned with the existing web-control transaction model.

## Home Assistant entity shape

The facade advertises one segment with:

    info.leds.maxseg = 1
    info.leds.lc     = 2
    info.leds.seglc  = [2]
    segment bri      = 255

In the current Home Assistant WLED integration, capability 2 maps to
ColorMode.BRIGHTNESS. The controller therefore appears as a dimmable light
without exposing an RGB picker that would conflict with realtime DDP colors.

WLED effects remain limited to Solid.

## HyperHDR

Use the HyperHDR Hyperk device type when this compatibility facade is used for
device discovery/control.

DriverNetHyperk:

    inherits DriverNetDDP
    GETs /json
    PUTs /json/state
    sends realtime RGB by DDP

This is the transport model implemented by this firmware.

Do not configure the controller as a normal HyperHDR WLED realtime device and
do not infer support for WLED realtime UDP port 21324. The firmware's active
realtime transport remains:

    DDP UDP/4048

No WLED realtime UDP protocol is implemented or advertised.

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

Ambilight-specific configuration remains under the native /api/* surface.

## Resource model

The WLED parser and serializer are fixed-buffer, heap-free C++ code.

WebUiProtocol allows a larger body only for WLED JSON writes. Existing native
Ambilight /api payloads keep their tighter bounded contract.

DDP polling remains ahead of the HTTP service in the main loop.

## Validation

Native tests cover:

- WLED JSON state parsing
- nested/unknown JSON skipping
- brightness and on/off resolution
- segment on/off behavior
- current HA brightness capability shape
- state JSON projection
- info JSON fields used by HA and HyperHDR
- combined /json response body
- empty presets response
- response-buffer overflow failure
- HTTP GET/POST/PUT route and content-type rules
- legacy output-state migration
- atomic output-state write failure behavior

Full ESP32-C6 compilation and live Home Assistant/HyperHDR discovery remain
hardware/local validation gates and must not be inferred from native/static
source review.
