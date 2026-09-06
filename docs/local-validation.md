# Local validation harness

## Purpose

GitHub Actions are currently manual-only and repository Actions quota is exhausted.

Stage 31 provides local commands that mirror the two CI gates:

1. native unit tests
2. ESP32-C6 firmware build

Both gates are attempted in one run so a native-test failure does not hide a separate firmware compile failure.

## Required tools

PlatformIO must already be installed.

The scripts try these launchers in order.

Windows PowerShell:

    pio
    py -m platformio
    python -m platformio

Linux/macOS shell:

    pio
    python3 -m platformio
    python -m platformio

The scripts deliberately do not install or upgrade PlatformIO automatically.

## Windows

From the repository root:

    powershell -ExecutionPolicy Bypass -File tools/validate.ps1

PowerShell 7 also works:

    pwsh -File tools/validate.ps1

Skip native tests:

    powershell -ExecutionPolicy Bypass -File tools/validate.ps1 -SkipNative

Skip firmware build:

    powershell -ExecutionPolicy Bypass -File tools/validate.ps1 -SkipFirmware

## Linux/macOS

Run explicitly through bash so the Git executable bit is not required:

    bash tools/validate.sh

Skip native tests:

    bash tools/validate.sh --skip-native

Skip firmware build:

    bash tools/validate.sh --skip-firmware

## Commands mirrored from CI

Native:

    pio test -e native

Firmware:

    pio run -e esp32-c6-devkitc-1

The launcher may be python -m platformio instead of pio depending on the local installation.

## Artifacts

Each run creates:

    .artifacts/validation/YYYYMMDD-HHMMSS/

Files:

    native-test.log
    firmware-build.log
    summary.txt

The directory is excluded by .gitignore.

summary.txt contains:

- timestamp
- skipped gates
- native exit code
- firmware exit code
- artifact directory

## Exit status

The harness exits:

    0

only when every non-skipped gate succeeds.

It exits:

    1

when either validation gate fails.

Launcher/argument setup errors use a non-success exit as well.

## Important status

The harness was exercised on Windows during Stage 35.

Validated Stage 35 result:

    native:   147 passed, 0 failed
    firmware: ESP32-C6 build passed

The Windows PowerShell wrapper was fixed so native stderr does not terminate
the script before the firmware gate. Both gate exit codes are now collected
and written to summary.txt.

Stage 38 changes runtime DDP frame sizing, LED topology, ToF perimeter
sampling, serial protocol, web commissioning and transient ToF debug cadence.
It is not considered validated until a fresh harness run passes both native
and firmware gates.

The target hardware has 16 MB flash. Before flashing, Codex/local validation
must adapt or verify the intended partition table for that flash size and
record:

- native test total/pass/fail
- firmware build result
- RAM usage
- firmware.bin / factory image sizes
- actual application partition size and percentage

Do not compare Stage 38 against the historical Stage 35 1.31 MB app partition
as if it were still the deployment limit.

Preserve the generated artifact directory whenever a gate fails.
