# Stage 39 - pre-flash hardening

## Scope

Stage 39 does not add user-facing Ambilight features. It hardens the Stage 38
commissioning build before the first real hardware flashing/commissioning pass.

## Closed blockers

### Physical PARLIO tail cleanup

PARLIO lanes keep a fixed 230-pixel buffer even when a runtime side uses fewer
addresses.

LedRenderer now clears all physical lane buffers whenever LedMappingProfile
actually changes. This prevents RGB from an older/longer topology surviving in
inactive physical addresses after COUNT shrink or GPIO remap.

The clear is topology-change-only. Normal RGB renders do not pay a 920-pixel
clear on every frame.

### Coordinated topology apply/reset

Runtime topology activation now:

1. validates brightness/profile
2. queues ToF topology first
3. changes DDP expected frame length
4. changes renderer mapping
5. prepares an effective pixel mask without mutating persistence
6. commits custom topology or durable reset
7. persists required mask sanitation only after topology commit
8. invalidates cached RGB/gain state

Unexpected DDP/renderer rejection requests rollback to the previous topology.

A durable reset failure restores the previous live topology and mask.

### Durable reset markers

Version/count/SSID commit markers are removed before live state is reset.

Covered runtime data:

- Wi-Fi credentials: SSID is the commit marker
- ToF gain curve: point count is the commit marker
- ToF spatial profile: schema version is the commit marker
- LED topology: schema version is the commit marker
- disabled-pixel mask: schema version is the commit marker

If stale data-blob cleanup fails after its marker was removed, the stale blob
is inert on reboot and cannot resurrect the old setting.

### 16 MiB deployment layout

Committed partition table:

    partitions/ambilight_16mb_ota.csv

Layout:

    NVS        0x009000..0x00DFFF
    OTA data   0x00E000..0x00FFFF
    app0       0x010000..0x710000   7 MiB
    app1       0x710000..0xE10000   7 MiB
    storage    0xE10000..0xFF0000
    coredump   0xFF0000..0x1000000  64 KiB

PlatformIO is pinned to 16 MiB flash and a 7 MiB maximum application image.

## Validation gate

Run one of:

    powershell -ExecutionPolicy Bypass -File tools/validate.ps1

or:

    bash tools/validate.sh

A Stage 39 release candidate is accepted only when all three are zero:

    partition_exit=0
    native_exit=0
    firmware_exit=0

The partition gate checks alignment, overlap, 16 MiB bounds and both 7 MiB
application slots.

Record from the firmware build:

- final binary/program size
- RAM usage
- 7 MiB app-slot fit

## First hardware gate after software validation

Keep correction in DISABLED or SHADOW until ToF orientation is verified.

Recommended order:

1. brightness 0, verify/save COUNT/GPIO/REV topology
2. brightness <=64, run raw GPIO ranges
3. run logical whole-side/range tests
4. confirm disabled-pixel mask
5. run ToF live debug and verify normalized TOP/LEFT orientation
6. capture calibration data
7. verify SHADOW gain telemetry
8. enable ACTIVE only after fail-open and geometry behavior are confirmed
