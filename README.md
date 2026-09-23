# Ambilight ESP32-C6

[English](README.md) | [Русский](README_RU.md)

[![Latest release](https://img.shields.io/github/v/release/VooxergAlt2/ambilight)](https://github.com/VooxergAlt2/ambilight/releases/latest)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A DIY ESP32-C6 Ambilight controller built for low-latency HyperHDR output, with optional VL53L5CX wall-distance correction, a small built-in Web UI, and Home Assistant control through a deliberately limited WLED-compatible facade.

This project grew out of a real TV installation rather than a generic LED-controller framework. The firmware keeps the realtime RGB path small and deterministic, while moving commissioning, topology, diagnostics, Wi-Fi recovery, and distance compensation into the controller.

> **Project status:** actively developed and hardware-tested on the author's installation. It is usable, but still an enthusiast firmware rather than a polished consumer product. Read the hardware notes before flashing or connecting LEDs.

## What it does

- receives realtime RGB from **HyperHDR over DDP UDP/4048**
- drives **4 synchronized LED outputs** using the ESP32-C6 PARLIO peripheral
- has **no separate fixed aggregate LED-count ceiling**; runtime buffers scale to the active topology
- keeps LED side length, GPIO assignment, and direction configurable at runtime
- provides safe logical-side and raw-GPIO commissioning tests
- can force one physical LED per side permanently black, useful for a bad pixel without changing logical indexing
- supports an optional **VL53L5CX 8x8 ToF sensor** for wall-plane and per-pixel distance correction
- provides DISABLED / SHADOW / ACTIVE correction modes with fail-open behavior
- exposes a lightweight embedded Web UI for normal setup and diagnostics
- exposes power, source-aware brightness, RGB, and native lighting effects to **Home Assistant** through WLED-compatible discovery/API
- stores runtime settings in NVS
- preserves unknown settings records across firmware upgrade/downgrade boots
- exports/restores a versioned JSON configuration backup from the Web UI
- opens a recovery Wi-Fi AP after 60 seconds without a station connection
- supports guarded **Wi-Fi OTA** of `firmware.bin` into the inactive 7 MiB application slot while preserving NVS

HyperHDR remains independent from the WLED compatibility layer. Realtime video always uses DDP.

## Architecture at a glance

```text
HyperHDR
   |
   | DDP UDP/4048
   v
ESP32-C6
   +--> frame assembly / sender isolation
   +--> optional ToF correction
   +--> AUTO DDP/local owner arbitration + separate brightness banks
   +--> physical pixel mask
   +--> PARLIO x4
   v
LED strips

Home Assistant
   |
   +--> mDNS _wled._tcp
   +--> HTTP/80 WLED-compatible JSON
          power / brightness / RGB / local effects

Browser
   |
   +--> HTTP/80 native Ambilight Web UI
```

There is intentionally **no WLED realtime UDP transport** and no HyperHDR Hyperk emulation. Configure HyperHDR as a DDP device.

## Hardware

### Required

- an **ESP32-C6** board compatible with the PlatformIO `esp32-c6-devkitc-1` target
- **16 MiB flash** for the committed partition layout
- addressable LEDs compatible with the WS2812/WS2812B GRB timing used by LiteLED
- a correctly sized LED power supply
- a shared ground between the ESP32-side logic and LED data reference

The four firmware LED outputs are:

| PARLIO lane | GPIO |
| --- | ---: |
| 0 | 18 |
| 1 | 19 |
| 2 | 20 |
| 3 | 21 |

The default topology matches the author's 65-inch installation and can be changed from the Web UI or serial console:

| TV side | Default count | GPIO | Direction |
| --- | ---: | ---: | --- |
| Top | 230 | 20 | REV |
| Right | 160 | 19 | REV |
| Bottom | 230 | 21 | REV |
| Left | 160 | 18 | FWD |

There is no separate 920-address firmware ceiling. Each side length is stored as a
16-bit value (`1..65535`), so the four-side format can represent up to 262,140
logical LEDs in total, while RGB/DDP/gain/PARLIO storage is sized from the active
topology at runtime. The practical limit is therefore the memory and
timing budget of the ESP32-C6 rather than an arbitrary project constant.

**Hardware validation has been performed up to 230 LEDs on a single output.**
Larger per-output configurations are accepted by the software path and covered
by host-side large-topology tests, but have not yet been validated on a real LED
strip.

Do not assume those side assignments match your wiring. Use the commissioning tools to identify each physical output first.

A proper unidirectional logic-level buffer is strongly recommended between a 3.3 V ESP32 data output and LEDs that expect 5 V logic. Power the LED load directly from an appropriately sized supply, not through the ESP32 board.

### Optional ToF sensor

The distance-correction path uses a **VL53L5CX**:

| Signal | GPIO |
| --- | ---: |
| SDA | 6 |
| SCL | 7 |

Basic DDP Ambilight operation continues if the ToF sensor is absent or fails to initialize. ToF-dependent correction simply fails open.

## Build

Requirements:

- Python 3
- PlatformIO

Clone and validate:

```bash
git clone https://github.com/VooxergAlt2/ambilight.git
cd ambilight
python -m pip install --upgrade platformio
bash tools/validate.sh
```

On Windows PowerShell:

```powershell
python -m pip install --upgrade platformio
powershell -ExecutionPolicy Bypass -File tools/validate.ps1
```

The validation harness checks:

1. the 16 MiB partition layout
2. the embedded Web UI structure
3. all native unit tests
4. the ESP32-C6 firmware build

Direct PlatformIO commands:

```bash
pio test -e native
pio run -e esp32-c6-devkitc-1
```

Flash a connected board:

```bash
pio run -e esp32-c6-devkitc-1 -t upload
```

Serial monitor:

```bash
pio device monitor -b 115200
```

**Update warning:** use the application `firmware.bin` for an already configured controller, either through the Web UI OTA flow or at flash offset `0x10000`. The generated
`firmware.factory.bin` is a combined blank-device image and must not be used as
a normal update image if you want to keep NVS settings. A whole-chip erase has
the same destructive effect.

## First boot and Wi-Fi recovery

Wi-Fi credentials are resolved in this order:

1. credentials saved in NVS
2. optional compile-time credentials from `include/secrets.h`
3. no usable station connection

If the controller does not obtain a station connection for 60 seconds, it opens:

```text
SSID:     Ambilight-XXXXXX
password: ambilight
IP:       4.3.2.1
Web UI:   http://4.3.2.1/
```

The suffix is derived from the controller identity so multiple devices do not all advertise the same SSID.

STA reconnect attempts continue while the fallback AP is active. Once the configured network connects successfully, the AP closes automatically.

You can also provision from serial:

```text
w                       show Wi-Fi status
wSSID|PASSWORD          save and connect
wclear                  clear saved credentials
```

For optional compile-time credentials, copy `include/secrets.example.h` to `include/secrets.h`. The real file is ignored by Git.

## HyperHDR / Windows PC integration

No separate custom Windows plugin is required. Install **HyperHDR** on the PC
and configure this controller as a standard **DDP** network LED device. HyperHDR
does screen capture and realtime RGB generation; the ESP32-C6 owns LED topology,
physical output, commissioning and optional ToF correction.

Configure the ESP32 as a **DDP** LED device:

```text
Protocol: DDP
Port:     UDP/4048
LEDs:     sum of the four active side counts
```

The active RGB payload is always `totalLedCount * 3` bytes.

The firmware holds the last complete DDP frame through short transport gaps. In `Ambilight` AUTO mode, DDP remains visible while the stream is fresh; after **1.5 s without a complete frame** output switches to the configured local fallback effect. A fresh DDP frame automatically returns ownership to Ambilight. Partial packets never synthesize a black frame, while a real complete black frame remains authoritative.

Do **not** configure this firmware as a WLED realtime device or HyperHDR Hyperk device.

For the Windows setup flow, sender-ownership nuances, and release-binary flashing
commands, see [Windows: HyperHDR integration and firmware installation](docs/windows-hyperhdr.md).

## Home Assistant

The controller advertises `_wled._tcp.local.` and implements the subset of WLED JSON needed for the Home Assistant WLED integration.

The WLED-compatible Home Assistant surface supports:

- on/off;
- brightness from both master and segment-0 commands;
- RGB color;
- `Ambilight` AUTO mode;
- `Solid`, `Rainbow`, `Breathing`;
- `Warm White`, `Bias White`, `Sunset`, `Candle`, `Aurora`, `Twinkle`;
- effect speed (`sx`) and intensity (`ix`).

`Ambilight` is now AUTO ownership: fresh DDP owns the LEDs, stale DDP falls back to the remembered local effect after 1.5 s, and the next fresh DDP frame takes ownership back automatically. Explicit local effects override DDP while still allowing DDP reception in the background.

This is a compatibility facade, not a WLED fork. Unsupported WLED features are documented in [docs/wled-ha-compat.md](docs/wled-ha-compat.md).

## Web UI

Open:

```text
http://<controller-ip>/
```

The UI includes:

- **Home** - power, separate DDP/local brightness, AUTO/fallback lighting, effects, correction, DDP and ToF summary
- **LED** - side topology, GPIO identification, direction/range tests, disabled-pixel mask
- **ToF** - 8x8 live matrix, geometry, gain curve, calibration
- **Diagnostics** - transport, heap, ToF, and renderer counters
- **System** - Wi-Fi provisioning, guarded Wi-Fi OTA, configuration backup/restore, and factory reset

The UI is intentionally lightweight: no external assets, no WebSocket, and no web framework.

There is currently no Web UI authentication. Treat it as a trusted-LAN interface and do not publish TCP/80 to the internet.

### Configuration backup and firmware updates

The System page can download a versioned JSON backup of the current runtime
configuration and restore it later. Backup v2 includes output power, separate DDP/local brightness, the local lighting/fallback profile, correction mode, LED topology, disabled-pixel mask, ToF geometry, gain curve, and the Wi-Fi SSID. Backup v1 remains accepted. The saved Wi-Fi password is deliberately excluded and must be entered
again if the restored setup uses a different network.

Normal application updates preserve the dedicated NVS settings partition. The
boot loader for runtime settings is also non-destructive: records with an
unknown schema are left intact instead of being deleted merely because the
running firmware cannot understand them. This keeps downgrade/recovery and
future migrations possible.

For Wi-Fi OTA, arm the update from **System**, select the release `firmware.bin`, and upload it within the 120-second one-time-token window. The controller validates an ESP32-C6 application header before writing the inactive OTA slot, blacks the LEDs while flash is being written, then reboots on success. `firmware.factory.bin`, bootloader images, and non-C6 applications are rejected by the OTA path.

A deliberate whole-chip erase or flashing the combined factory image over an existing device still removes NVS. Download a backup first if you plan to do either. See [Wi-Fi OTA](docs/wifi-ota.md).

## ToF distance correction

The VL53L5CX is used as a slow geometry sensor rather than part of the realtime RGB path.

The firmware estimates a wall plane and evaluates a gain for each perimeter LED:

```text
distance_i = wall_plane(x_i, y_i) - led_z_i
gain_i     = configured_curve(distance_i)
```

Correction never depends on ToF being healthy for basic Ambilight operation. Invalid, stale, or unavailable geometry fails open to the original HyperHDR RGB.

The three runtime modes are:

- **DISABLED** - no correction
- **SHADOW** - calculate and expose correction without changing physical RGB
- **ACTIVE** - apply correction with bounded slew

See [docs/tof-processing.md](docs/tof-processing.md), [docs/tof-plane-fit.md](docs/tof-plane-fit.md), and [docs/spatial-perimeter-gains.md](docs/spatial-perimeter-gains.md).

## Commissioning

The project includes tools specifically for wiring a real TV without recompiling for every mistake:

- raw GPIO probes identify which strip is physically connected to GPIO 18/19/20/21
- logical-side probes verify side mapping and REV/FWD
- topology can be edited as COUNT/GPIO/REV
- one service/disabled physical LED per side can be inserted as a black wire-address hole; logical pixels shift around it without changing DDP/ToF logical counts
- the ToF page shows the normalized 8x8 grid relative to the TV

Commissioning patterns are brightness-limited in firmware.

See [docs/led-commissioning-patterns.md](docs/led-commissioning-patterns.md) and [docs/runtime-led-mapping.md](docs/runtime-led-mapping.md).

## Current validation

The current development line is validated with:

- partition check: PASS
- embedded Web UI structural check: PASS
- native tests: **272 / 272 PASS**
- ESP32-C6 build: PASS
- RAM: **70,460 / 327,680 bytes (21.5%)**
- application image: **1,342,096 / 7,340,032 bytes (18.3%)**

Exact numbers can move between commits. A fresh `tools/validate.*` run is authoritative; the manual GitHub Actions workflows mirror the same gates when hosted runners are available.

## Documentation

Start here:

- [Architecture](docs/architecture.md)
- [Local validation](docs/local-validation.md)
- [Runtime Wi-Fi](docs/runtime-wifi.md)
- [Web UI](docs/web-ui.md)
- [WLED / Home Assistant compatibility](docs/wled-ha-compat.md)
- [Runtime LED mapping](docs/runtime-led-mapping.md)
- [Runtime configuration recovery](docs/runtime-config-recovery.md)
- [Windows / HyperHDR installation](docs/windows-hyperhdr.md)
- [Wi-Fi OTA](docs/wifi-ota.md)
- [ToF processing](docs/tof-processing.md)

The `docs/` directory also keeps stage-specific engineering notes. Those files are useful historical context, but this README and the current code are authoritative for the present firmware.

## Contributing

Bug reports, hardware observations, documentation fixes, and focused pull requests are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting changes.

If you find a security issue, see [SECURITY.md](SECURITY.md).

## License

The project is released under the [MIT License](LICENSE).

Third-party components retain their own licenses. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and the license files bundled with vendored code.

## Disclaimer

This is an independent hobby project. It is not affiliated with or endorsed by HyperHDR, WLED, Home Assistant, Espressif, STMicroelectronics, or Adafruit.

You are responsible for electrical safety, power sizing, grounding, fusing, thermal behavior, and verifying that your particular LED hardware is compatible before connecting it.
