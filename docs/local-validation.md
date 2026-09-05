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

The harness scripts are part of the repository and have been statically reviewed.

They have not been executed in this development session.

Therefore Stage 31 does not imply that the current native tests or ESP32-C6 firmware build have passed.

The first real local validation run should preserve the generated artifact directory for review if any gate fails.
