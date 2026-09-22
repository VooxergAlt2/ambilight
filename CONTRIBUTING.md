# Contributing

Thanks for helping improve Ambilight ESP32-C6.

This is a hardware-facing firmware project, so a small change can affect realtime
DDP transport, LED timing, persistent configuration, commissioning safety, or
physical output. Please keep changes focused and preserve those boundaries.

## Before opening an issue

For bugs, include as much of the following as is relevant:

- firmware commit or version
- ESP32-C6 board and flash size
- LED type, count, and power arrangement
- active COUNT/GPIO/REV topology
- whether VL53L5CX is connected
- HyperHDR version and DDP configuration
- Home Assistant version if the WLED facade is involved
- serial log around the failure
- exact reproduction steps

Do not post Wi-Fi passwords, tokens, private keys, or other credentials.

## Development setup

Install PlatformIO, then run the complete local gate from the repository root:

```bash
python -m pip install --upgrade platformio
bash tools/validate.sh
```

Windows:

```powershell
python -m pip install --upgrade platformio
powershell -ExecutionPolicy Bypass -File tools/validate.ps1
```

The full gate must pass before a pull request is considered ready:

1. partition layout check
2. embedded Web UI structural check
3. native unit tests
4. ESP32-C6 firmware build

## Pull requests

Prefer one coherent change per pull request.

Please:

- branch from `main`
- add native regression coverage for pure logic whenever practical
- keep fixed-size / heap-free behavior in realtime paths unless there is a measured reason to change it
- avoid adding a second configuration path for an existing setting
- update documentation when behavior or user-facing contracts change
- preserve fail-open behavior for optional ToF correction
- preserve DDP UDP/4048 as the HyperHDR realtime transport
- keep WLED support limited to the documented Home Assistant compatibility facade unless a broader change is explicitly proposed
- do not commit `include/secrets.h`, build output, validation artifacts, or local IDE files

For changes that touch physical LED output, describe how the change was tested on
hardware, or clearly state that hardware validation is still pending.

## Code style

Match the surrounding C++ style rather than reformatting unrelated code.

Warnings from upstream ESP-IDF / Arduino headers are expected under the current
pedantic flags. New warnings introduced by project code should be avoided.

## Tests

Useful direct commands:

```bash
pio test -e native
pio run -e esp32-c6-devkitc-1
python tools/check_partition.py
python tools/check_web_ui.py
```

## Third-party code

Do not copy third-party code into the repository without preserving its license
and attribution. Update `THIRD_PARTY_NOTICES.md` when adding a bundled
dependency.

## Security

Please do not publish exploit details or credentials in a normal issue. Follow
[SECURITY.md](SECURITY.md) instead.
