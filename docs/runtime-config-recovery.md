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

Factory reset is accepted only when the effective physical output brightness is
zero:

    effective brightness = 0

This is true when output power is off or when configured brightness is zero.
The check therefore follows what the LED driver is actually emitting rather
than only the remembered brightness value.

This prevents an accidental reset while live LEDs are visibly active.

Before clearing NVS the firmware also performs a best-effort physical LED blackout.

## Transaction order

The reset deliberately clears persistent storage before changing the RuntimeSettings in-memory view.

Sequence:

1. verify effective physical brightness=0
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

Output state:

    enabled
    brightness 32/255

Wi-Fi NVS credentials:

    absent

Wi-Fi startup therefore falls back to:

    secrets.h

if compiled. If no station connection is available for 60 seconds, the
fallback AP is opened for recovery/provisioning.

ToF gain curve:

    default neutral curve

ToF spatial profile:

    default 1437.5 x 1000 mm profile

LED mapping:

    TOP     230 -> GPIO20 REV
    RIGHT   160 -> GPIO19 REV
    BOTTOM  230 -> GPIO21 REV
    LEFT    160 -> GPIO18 FWD

## What is not erased

Factory reset clears only the Preferences namespace:

    ambilight

It does not erase:

- firmware
- flash partitions outside this namespace
- compile-time secrets.h
- USB/AWA branch work

## Firmware updates and NVS preservation

Runtime configuration lives in the dedicated `nvs` data partition at
`0x9000..0xDFFF`. Application images live in `app0` / `app1`; replacing an
application image or switching OTA slots therefore does not require erasing
the settings partition.

Normal firmware updates must preserve NVS. Do not add an automatic
`Preferences::clear()`, partition erase, or whole-chip erase to an update path.
Only the explicit guarded factory-reset flow is allowed to clear the
`ambilight` namespace.

Boot loading is deliberately non-destructive. If a stored setting has an
unknown schema version or cannot be applied by the running firmware, the
controller uses a safe runtime fallback but leaves the stored bytes intact.
This matters for forward upgrades, downgrade/recovery, and future migrations:
an older firmware must not destroy a record merely because it does not
understand it.

Supported migrations may still replace an old representation after the new
representation has been durably written. That is a migration, not boot-time
self-healing.

An explicit full-flash/chip erase still destroys NVS. Back up configuration
before doing that.

### Update image versus factory image

Use the application image (`firmware.bin`) or a future OTA app-slot update when
updating an already configured controller. Those paths replace application code
without intentionally erasing the dedicated NVS partition.

`firmware.factory.bin` is a combined first-install/factory image that starts at
flash offset 0 and includes padding across the early data-partition area. Do
**not** flash that combined image over an existing configured controller when
you want to preserve NVS. Treat it as a blank-device provisioning image.

## Configuration backup

The Web UI System page can download a versioned JSON backup and restore it
later.

The backup contains:

- output power and brightness
- correction mode
- LED COUNT/GPIO/REV topology
- disabled-pixel mask
- ToF spatial geometry
- ToF gain curve
- Wi-Fi SSID as a reference

The saved Wi-Fi password is intentionally **not** returned to the browser and
is not placed in the backup file. Restore therefore leaves the active network
connection unchanged. If the backup references another SSID, enter its
password in the Wi-Fi form and save it separately after restoring the other
settings.

Restore validates the backup format and then applies settings through the same
runtime API used by normal Web UI edits. It first switches physical output off,
then selects SHADOW mode so ToF geometry/curve changes are not rejected by
ACTIVE-mode guards. The original correction mode is restored near the end and
the requested output-power state is restored last. If any step is rejected,
restore stops and leaves physical output off unless the final power step had
already completed.
