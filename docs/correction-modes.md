# Runtime correction modes

## Purpose

The firmware has an explicit runtime boundary between geometry/gain calculation and physical LED output.

Correction mode is persistent in ESP32 NVS.

Default when no valid saved value exists:

    SHADOW

## Modes

### DISABLED

    !0

Behavior:

- HyperHDR RGB goes to LEDs unchanged
- ToF service may continue collecting geometry/diagnostics
- main does not poll/apply the active per-LED gain target
- gain-only rerenders are suppressed
- switching into DISABLED forces one render of cached RGB when available

Use this to remove correction overhead from the render path without disabling Ambilight itself.

### SHADOW

    !1

Behavior:

- ToF plane/gain pipeline is active
- active per-LED target is copied into render state
- gain slew and candidate RGB are calculated
- physical LEDs still receive original HyperHDR RGB

This is the default development/diagnostic mode.

### ACTIVE

    !2

Behavior:

- same plane/gain pipeline as SHADOW
- after fail-open and slew handling, candidate RGB is sent to physical LEDs
- invalid/stale ToF still produces unity and therefore original RGB

## Mode status

    m

prints current mode, NVS availability and persistence counters.

Mode-switch commands deliberately require the `!` prefix. Bare digits are ignored so pasted logs or ordinary numeric serial traffic cannot accidentally enable ACTIVE mode.

## Transition behavior

Every correction-mode change resets RenderGainController.

Therefore entering ACTIVE starts from unity and then slews toward the valid target.

This prevents an already-attenuated shadow state from being applied to LEDs in one abrupt frame.

Leaving ACTIVE returns physical output to original HyperHDR RGB immediately.

## Debug probe safety

The synthetic `x` gain probe is accepted only in SHADOW mode.

It is refused in DISABLED and ACTIVE.

Any correction-mode transition also cancels an existing probe.

Therefore the deliberately aggressive debug profile cannot reach physical LEDs through ACTIVE mode.

## Persistence

NVS namespace:

    ambilight

Key:

    corr_mode

Stored values:

    0 = DISABLED
    1 = SHADOW
    2 = ACTIVE

Invalid stored values fall back to SHADOW.

If NVS is unavailable, runtime mode switching still works for the current boot but is reported as non-persistent.

## Fail-open contract

ACTIVE does not bypass the existing safety chain.

If the spatial source is absent, stale, invalid or out of range:

    RenderGainContext.failOpen = true
    gainForLogicalIndex() = unity
    candidate RGB = original RGB
    ACTIVE physical output = original RGB

This contract is covered by native tests.
