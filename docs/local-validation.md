# Local validation harness

## Purpose

The local harness is the primary release gate. The manual GitHub Actions workflows
mirror the same checks when hosted runners are available, so validation can be
repeated remotely without changing the test contract.

The complete validation path is:

1. 16 MiB partition-layout check
2. embedded Web UI structural check
3. native unit tests
4. ESP32-C6 firmware build

All gates are attempted in one run so an early failure does not hide a separate
firmware compile failure.

## Required tools

PlatformIO must already be installed.

Typical install:

```bash
python -m pip install --upgrade platformio
```

The scripts try the usual `pio` and `python -m platformio` launchers.

## Windows

From the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File tools/validate.ps1
```

PowerShell 7:

```powershell
pwsh -File tools/validate.ps1
```

Optional skips:

```powershell
powershell -ExecutionPolicy Bypass -File tools/validate.ps1 -SkipNative
powershell -ExecutionPolicy Bypass -File tools/validate.ps1 -SkipFirmware
```

## Linux / macOS

```bash
bash tools/validate.sh
```

Optional skips:

```bash
bash tools/validate.sh --skip-native
bash tools/validate.sh --skip-firmware
```

## Direct commands

Partition layout:

```bash
python tools/check_partition.py
```

Embedded Web UI:

```bash
python tools/check_web_ui.py
```

Native tests:

```bash
pio test -e native
```

Firmware:

```bash
pio run -e esp32-c6-devkitc-1
```

## Artifacts

Each harness run creates:

```text
.artifacts/validation/YYYYMMDD-HHMMSS/
```

Files include:

```text
partition-check.log
web-ui-check.log
native-test.log
firmware-build.log
summary.txt
```

The directory is ignored by Git.

The harness returns success only when every non-skipped gate succeeds.

## Current baseline

The public-preparation baseline completed successfully with:

```text
partition: PASS, 16 MiB flash, 7 MiB application slots
web UI:    PASS
native:    272 passed, 0 failed
firmware:  ESP32-C6 build passed
RAM:       70460 / 327680 bytes (21.5%)
Flash:     1342096 / 7340032 bytes (18.3%)
```

These values are a snapshot, not a compatibility contract. A fresh local
validation run is authoritative for the current commit; the manual hosted
workflow is an equivalent remote execution path when available.

Preserve the generated artifact directory whenever a gate fails because the
individual logs often make unrelated failures visible at the same time.
