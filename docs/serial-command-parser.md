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
    z/Z  toggle 60 s ToF live debug
    m/M  correction status
    v/V  firmware identity

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
    j...  LED commissioning range
    l...  LED runtime topology
    d...  disabled LED pixel mask
    y...  ToF spatial profile
    q...  ToF gain curve
    w...  Wi-Fi
    b...  brightness

CRLF produces only one event. The second newline is idle input and is ignored.

## Payload limits

The parser preserves the previous fixed-buffer limits:

    factory       15 chars
    LED range      47 chars
    LED topology   95 chars
    pixel mask     31 chars
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
    lbad-count-or-gpio
    fnot-reset

They are valid serial frames.

Their existing semantic handlers validate the payload and produce the user-facing error.

This keeps framing and subsystem rules separate.

## Stage 38 semantic payloads

The serial protocol version is now 2 because the meaning of `l` changed.

Topology:

    lCOUNT:GPIO:REV,COUNT:GPIO:REV,COUNT:GPIO:REV,COUNT:GPIO:REV

Example:

    l230:18:0,160:19:0,230:20:0,160:21:0

Range commissioning:

    jside:SIDE:START:COUNT
    jgpio:GPIO:START:COUNT

ToF live debug:

    z

toggles a transient 60-second session. It is refused in ACTIVE mode.

## Native contracts

test/test_serial_command_parser covers:

- idle newline/noise handling
- case-insensitive immediate selectors
- correction one-byte framing
- commissioning one-byte framing
- commissioning range line framing
- ToF debug immediate selector
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
