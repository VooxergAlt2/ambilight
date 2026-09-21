# Runtime output state

## Purpose

Stage 46 separates the remembered output brightness from the output power
state.

ToF correction answers:

    how much should each LED be attenuated because of wall distance?

Output brightness answers:

    what global brightness ceiling should be used when output is enabled?

Output power answers:

    should the physical LED output currently be enabled at all?

The three controls remain independent.

## Persistent representation

NVS namespace:

    ambilight

Stage 46 stores power and brightness together in one versioned binary record:

    output_state

Record schema:

    schemaVersion = 1
    brightness    = 0..255
    enabled       = 0 | 1

The record is written in one Preferences::putBytes() operation. Power and
brightness are therefore never persisted as two independently committed
settings.

Migration supports both older representations:

    Stage <=45:
        brightness

    early Stage 46:
        brightness
        output_on

Legacy keys are removed only after the new output_state record has been
successfully written. A failed migration can therefore be retried on the next
boot.

Default on first boot:

    enabled    true
    brightness 32/255

## Runtime semantics

The physical LiteLED brightness is:

    effectiveBrightness =
        enabled ? configuredBrightness : 0

Turning output off does not erase the remembered brightness.

Example:

    enabled=true,  brightness=120 -> physical 120
    enabled=false, brightness=120 -> physical 0
    enabled=true,  brightness=120 -> physical 120

This matches WLED/Home Assistant on/off semantics.

## Historical brightness command

The serial brightness command remains backward compatible:

    b0      -> output off, configured brightness becomes 0
    b1..255 -> output on and set configured brightness
    b       -> print current output state

The Web UI brightness slider follows the same legacy behavior.

The separate Web UI power control can switch output off while preserving the
configured brightness.

## WLED / Home Assistant behavior

The Stage 46 WLED compatibility facade exposes:

    state.on
    state.bri

Home Assistant may therefore switch the light off without destroying the last
non-zero brightness.

A WLED command:

    {"on":false}

turns output off and preserves brightness.

A later:

    {"on":true}

restores that brightness. If the remembered value is zero, firmware restores
the conservative firmware default instead of turning on at an invisible zero
level.

An explicit:

    {"bri":0}

is treated as an off request.

## Render behavior

Any effective output change marks render state dirty and re-renders the cached
RGB frame. No PARLIO reinitialization is required.

Brightness remains a final global multiplier in all correction modes:

    DISABLED
    SHADOW
    ACTIVE

ACTIVE ToF gain and global brightness therefore remain independent attenuation
layers.

## Failure behavior

If NVS is unavailable or the output_state write fails, the requested runtime
state still takes effect and the operation reports persistence failure.

Because the durable state is one record, a reboot can only recover the last
complete persisted output state. It cannot recover a new brightness paired
with an old power flag, or the reverse.

## Safety

The default 32/255 remains intentionally conservative until the actual strip,
power supply and thermal behavior are validated.

Factory recovery checks the effective physical brightness, not merely the
remembered configured brightness.
