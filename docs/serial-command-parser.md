# Serial command parser

## Purpose

Runtime configuration and diagnostics use the USB serial console.

Before Stage 32, command framing lived directly in main.cpp as:

- eight pending-state flags
- five fixed command buffers
- separate lengths
- a brightness digit accumulator
- one large nested serviceDebugCommands() state machine

Stage 32 moves framing into a pure C++ SerialCommandParser.

Arduino/Serial remains only the byte source.

## Runtime path

    Serial.read()
      -> SerialCommandParser::feed(byte)
      -> SerialCommandEvent
      -> dispatchSerialCommand()
      -> semantic handler

The parser does not know about:

- Wi-Fi
- NVS
- LEDs
- ToF
- Arduino
- esp_timer

It only owns command framing.

## Immediate commands

These emit an event as soon as the selector byte is received:

    t/T  raw ToF map
    g/G  ToF geometry
    p/P  wall plane
    k/K  legacy gains
    s/S  spatial gains
    c/C  calibration capture
    r/R  render diagnostics
    x/X  shadow probe
    m/M  correction status

## One-argument commands

Correction:

    !0
    !1
    !2

The parser waits for exactly one byte after !.

The historical:

    !<Enter>

behavior remains a no-op.

Commissioning:

    i0
    i1
    i2
    i<Enter>

The parser waits for exactly one byte after i/I.

## Line commands

These collect printable ASCII until CR or LF:

    f...  factory recovery
    l...  LED mapping
    y...  ToF spatial profile
    q...  ToF gain curve
    w...  Wi-Fi
    b...  brightness

CRLF produces only one event. The second newline is idle input and is ignored.

## Payload limits

The parser preserves the previous fixed-buffer limits:

    factory       15 chars
    LED map       63 chars
    spatial      127 chars
    gain curve   127 chars
    Wi-Fi         96 chars
    brightness     3 chars

All emitted payloads are NUL terminated.

The Wi-Fi limit exactly accommodates:

    32-char SSID + "|" + 63-char password

## Framing errors

Line payload accepts printable ASCII 32..126.

A line enters an error state on:

- unsupported control byte
- payload overflow

One error event is emitted.

The parser then discards every remaining byte until CR/LF.

This is intentionally safer than the old main.cpp implementation.

Example broken input:

    q50<control>x<Enter>

The x is discarded as part of the damaged q line.

It cannot accidentally become:

    x = start shadow probe

After the line terminator, normal command parsing resumes.

## Semantic validation remains outside

The parser does not decide whether these are meaningful:

    b999
    qbad-data
    ybad-data
    wmissing-separator
    lduplicate-lanes
    fnot-reset

They are valid serial frames.

Their existing semantic handlers validate the payload and produce the user-facing error.

This keeps framing and subsystem rules separate.

## Native contracts

test/test_serial_command_parser covers:

- idle newline/noise handling
- case-insensitive immediate selectors
- correction one-byte framing
- commissioning one-byte framing
- CRLF behavior
- empty status line commands
- full 32+63 Wi-Fi payload
- line prefix characters inside payload
- 3-digit brightness boundary
- brightness overflow
- unsupported control bytes
- discard-until-EOL safety
- 127-byte gain payload boundary
- overflow error behavior
- explicit parser reset

The parser source is included in the native PlatformIO build filter.

## Validation status

The tests are committed as deterministic native contracts.

They have not been executed in this development session.

Use:

    powershell -ExecutionPolicy Bypass -File tools/validate.ps1

or:

    bash tools/validate.sh

when local PlatformIO validation is available.
