# LED commissioning patterns

## Purpose

The controller can verify physical segment assignment and strip direction without HyperHDR changes or a firmware rebuild.

Patterns are generated in logical screen space and rendered through the normal runtime LED mapping profile.

## Safety limit

Commissioning patterns run only when global output brightness is:

    1..64

At 0 they would be invisible.

Above 64 they are refused.

If brightness leaves this range while a test is active, the test is cancelled.

## Commands

Status:

    i<Enter>

Stop:

    i0

Segment identity:

    i1

Direction markers:

    i2

Each test runs for:

    15 seconds

## Segment identity pattern

Logical colors:

    TOP     red
    RIGHT   green
    BOTTOM  blue
    LEFT    white

This verifies:

- which physical connector/lane drives each side
- whether the runtime lane permutation is correct

## Direction pattern

Each logical segment is mostly dim gray.

Markers:

    START   red, first 5 LEDs
    MID     green, 5 LEDs around midpoint
    END     blue, last 5 LEDs

Because the pattern passes through the active LedMappingProfile, a reversed segment will physically swap the screen-space start/end positions exactly as configured.

## DDP coexistence

Commissioning owns physical LED output temporarily.

DDP continues to:

- receive packets
- maintain sender isolation
- assemble frames
- publish the newest frame to the mailbox

Normal HyperHDR rendering is suppressed only while the commissioning pattern is active.

Clean DDP backlog is not allowed to defer commissioning output.

## Return from test

At timeout or manual stop:

1. commissioning override ends
2. latest mailbox frame is copied
3. if RGB exists, it is forced dirty and rendered immediately
4. if no RGB frame exists, LEDs are explicitly rendered black

A test pattern therefore cannot remain stuck on screen after it expires.

## Correction behavior

Commissioning frames are rendered with:

    CorrectionMode::Disabled
    RenderGainContext::unity()

The test is intended to inspect physical wiring/mapping, not ToF photometric behavior.

The persisted correction mode itself is not changed.

## Runtime mapping workflow

Recommended first hardware checkout:

    b32
    i1

Confirm the four physical sides.

If lanes are wrong:

    b0
    l...
    b32
    i1

Then verify direction:

    i2

If one side is reversed:

    b0
    l... with that segment reversed=1
    b32
    i2
