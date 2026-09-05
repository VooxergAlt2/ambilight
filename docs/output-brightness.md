# Runtime output brightness

## Purpose

Global output brightness is a separate final multiplier from ToF correction.

ToF answers:

    how much should this LED be attenuated because of wall distance?

Output brightness answers:

    what overall maximum brightness should the whole installation use?

These controls are intentionally independent.

## Storage

NVS namespace:

    ambilight

Key:

    brightness

Range:

    0..255

Default on first boot:

    32

The conservative default remains intentionally low until the real LED strip, power supply and thermal behavior are validated.

## Serial command

Set an exact value:

    b128<Enter>

Range:

    b0
    ...
    b255

Print current value:

    b<Enter>

Bare numeric traffic does not alter brightness.

## Runtime behavior

Changing brightness:

1. stores the new value in NVS when available
2. updates LiteLED group brightness
3. marks output dirty
4. rerenders the cached RGB frame once

No PARLIO reinitialization is required.

## Relationship with correction mode

Brightness applies in all correction modes:

    DISABLED
    SHADOW
    ACTIVE

ACTIVE ToF gain and global brightness therefore compose as two independent attenuation layers.

## Safety

Brightness 0 is valid and produces a global blackout while retaining normal firmware/network state.

The default 32/255 is not a final recommended operating value. It is a safe first-boot ceiling until the installation power budget is known.
