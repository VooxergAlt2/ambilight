## What changed

Describe the behavior change and why it belongs in this project.

## Validation

- [ ] `python tools/check_partition.py`
- [ ] `python tools/check_web_ui.py`
- [ ] `pio test -e native`
- [ ] `pio run -e esp32-c6-devkitc-1`
- [ ] Hardware validation performed, or hardware validation is not applicable / is explicitly noted below

## Hardware / integration notes

Describe any impact on LED timing, GPIO mapping, power, ToF, HyperHDR, Home Assistant, Wi-Fi, persistence, or commissioning.

## Checklist

- [ ] No credentials, generated artifacts, or local `include/secrets.h` are included
- [ ] Tests cover new pure logic where practical
- [ ] Documentation is updated for user-visible behavior
- [ ] Unrelated code was not reformatted or refactored
