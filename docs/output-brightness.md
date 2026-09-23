# Runtime output state

## Purpose

Stage 47 keeps physical output power global while separating the remembered
brightness of the two normal LED sources:

- **DDP brightness** for Ambilight/DDP ownership;
- **lighting brightness** for explicit local effects and AUTO fallback.

ToF correction remains a separate per-pixel attenuation layer below source
selection.

## Persistent representation

NVS namespace:

    ambilight

Power and both brightness banks are committed atomically in `output_state`
schema 2:

    schemaVersion      = 2
    ddpBrightness      = 0..255
    lightingBrightness = 0..255
    enabled            = 0 | 1
    reserved           = 0

A known schema-1 record is migrated by copying its single brightness into both
new banks. Pre-Stage-46 `brightness` / `output_on` keys remain supported as
migration inputs. Unknown/future records are not rewritten on boot.

Default on first boot:

    enabled            true
    ddpBrightness      32/255
    lightingBrightness 32/255

The local lighting profile is a separate versioned `manual_light` record. It
stores selected mode, AUTO fallback effect, RGB, speed and intensity.

## Runtime semantics

The physical LiteLED brightness is chosen from the active owner:

    selectedBrightness =
        owner == DDP ? ddpBrightness : lightingBrightness

    effectiveBrightness =
        enabled ? selectedBrightness : 0

Owner changes therefore switch brightness banks without overwriting either
stored value. Turning global power off preserves both banks.

Example:

    ddp=180, lighting=42, AUTO + fresh DDP -> physical 180
    ddp=180, lighting=42, AUTO + stale DDP -> physical 42
    fresh DDP returns                         -> physical 180

## Legacy brightness command

The historical serial/native brightness command remains backward compatible:

    b0      -> output off and set both banks to 0
    b1..255 -> output on and set both banks to the same value
    b       -> print current output state

Likewise, legacy `POST /api/brightness` writes both banks. New Web UI controls
use `/api/ddp-brightness` and `/api/lighting-brightness` independently.

## WLED / Home Assistant behavior

WLED `state.on` remains global power. WLED master `bri` updates the brightness
bank belonging to the effect selected by the same resolved command:

- `Ambilight` -> DDP brightness;
- any local effect -> lighting brightness.

A plain power-off request preserves both remembered banks. If power-on is
requested when both banks are zero, firmware restores the conservative default
for both banks rather than creating an invisible on-at-zero state.

## Render behavior

Changing the active owner's brightness marks render state dirty; owner
transitions also apply the matching bank before the new source renders. No
PARLIO reinitialization is required.

Brightness remains the final whole-output multiplier in DISABLED, SHADOW and
ACTIVE correction modes. ACTIVE ToF gain and the selected brightness bank are
independent attenuation layers.

## Failure behavior

If NVS is unavailable or an `output_state` write fails, the requested runtime
state still takes effect and persistence failure is reported. Because all three
fields are one durable record, a reboot cannot recover a new brightness bank
paired with an old power flag or vice versa.

## Safety

Both defaults remain 32/255 until the actual strip, power supply and thermal
behavior are validated. Factory recovery checks effective physical brightness,
not either remembered bank in isolation.
