# Minimal web UI

## Purpose

Stage 36 adds a small LAN-only control surface for commissioning and normal
runtime operation.

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
    POST /api/spatial
    POST /api/curve
    POST /api/wifi
    POST /api/calibration
    POST /api/shadow-probe
    POST /api/factory

POST bodies use the same compact text payloads as existing runtime commands.

Examples:

    /api/brightness   32
    /api/correction   0 | 1 | 2
    /api/test         0 | 1 | 2
    /api/led-map      0:0,1:0,2:0,3:0
    /api/led-map      reset
    /api/spatial      1437.5,1000,0,0,0,0,0,10
    /api/spatial      reset
    /api/curve        50:2048,500:3072,4000:4096
    /api/curve        reset
    /api/wifi         SSID|PASSWORD
    /api/wifi         clear
    /api/calibration  start
    /api/shadow-probe start
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

This is important for:

- Wi-Fi credential changes
- clearing Wi-Fi credentials
- factory reset

Those actions may tear down the network or reboot the MCU.

## Safety contracts

The web UI does not weaken existing guards.

LED mapping:

    brightness = 0

Factory reset:

    brightness = 0

Commissioning patterns:

    brightness = 1..64

Spatial profile edits:

    not ACTIVE

Gain curve edits:

    not ACTIVE

Shadow probe:

    SHADOW only

Invalid web payloads are passed through the same existing typed runtime
parsers before a setting is applied.

## Calibration

The web UI can start the existing 60-second observational capture.

The latest CalibrationCaptureSummary is retained in RAM and returned by
/api/status so the result remains visible in the browser after capture
completion.

It is not persisted to NVS.

Starting a second capture while one is already active is refused.

## Status surface

The compact status endpoint includes:

- firmware version/stage
- correction mode and brightness
- Wi-Fi state, SSID, IP and RSSI
- DDP state and active sender
- ToF state/age/zones/median
- plane yaw/pitch/accepted zones
- perimeter min/max distance and fail-open state
- spatial profile
- gain curve
- runtime LED mapping
- commissioning state
- calibration state/result
- shadow probe state
- latest web action result
- heap diagnostics
- web request/action/error counters

Saved Wi-Fi password is never returned.

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

Stage 36 must not be treated as validated until both local gates pass:

    pio test -e native
    pio run -e esp32-c6-devkitc-1

Flash growth must be reviewed explicitly because the Stage 35 validated
application already used 91.6% of the current application partition.
