# Runtime configuration recovery

## Purpose

The firmware now stores multiple independent runtime profiles in the NVS namespace:

    ambilight

A single guarded recovery command restores the complete namespace to firmware defaults.

## Command

Factory reset:

    freset<Enter>

A plain:

    f<Enter>

does not reset anything.

It prints the required command instead.

## Safety condition

Factory reset is accepted only when:

    output brightness = 0

This prevents an accidental reset while live LEDs are visibly active.

Before clearing NVS the firmware also performs a best-effort physical LED blackout.

## Transaction order

The reset deliberately clears persistent storage before changing the RuntimeSettings in-memory view.

Sequence:

1. verify brightness=0
2. clear/show LED buffer
3. Preferences::clear() for namespace ambilight
4. only if clear succeeds, replace RuntimeSettings members with defaults
5. flush serial
6. restart ESP32

If NVS clear fails:

- no restart occurs
- live RuntimeSettings values remain unchanged
- firmware reports the failure

This avoids a half-reset state where RAM and flash disagree.

## Defaults after reboot

Correction mode:

    SHADOW

Output brightness:

    32/255

Wi-Fi NVS credentials:

    absent

Wi-Fi startup therefore falls back to:

    secrets.h

if compiled, otherwise Wi-Fi/DDP remains disabled until provisioned.

ToF gain curve:

    default neutral curve

ToF spatial profile:

    default 1437.5 x 1000 mm profile

LED mapping:

    TOP    lane0 FWD
    RIGHT  lane1 FWD
    BOTTOM lane2 FWD
    LEFT   lane3 FWD

## What is not erased

Factory reset clears only the Preferences namespace:

    ambilight

It does not erase:

- firmware
- flash partitions outside this namespace
- compile-time secrets.h
- USB/AWA branch work
