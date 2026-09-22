# Ambilight ESP32-C6

[![Build](https://github.com/VooxergAlt2/ambilight/actions/workflows/build.yml/badge.svg)](https://github.com/VooxergAlt2/ambilight/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A DIY ESP32-C6 Ambilight controller built for low-latency HyperHDR output, with optional VL53L5CX wall-distance correction, a small built-in Web UI, and Home Assistant control through a deliberately limited WLED-compatible facade.

This project grew out of a real TV installation rather than a generic LED-controller framework. The firmware keeps the realtime RGB path small and deterministic, while moving commissioning, topology, diagnostics, Wi-Fi recovery, and distance compensation into the controller.

> **Project status:** actively developed and hardware-tested on the author's installation. It is usable, but still an enthusiast firmware rather than a polished consumer product. Read the hardware notes before flashing or connecting LEDs.

## What it does

- receives realtime RGB from **HyperHDR over DDP UDP/4048**
- drives **4 synchronized LED outputs** using the ESP32-C6 PARLIO peripheral
- supports up to **4 x 230 physical LEDs**, 920 addresses total
- keeps LED side length, GPIO assignment, and direction configurable at runtime
- provides safe logical-side and raw-GPIO commissioning tests
- can force one physical LED per side permanently black, useful for a bad pixel without changing logical indexing
- supports an optional **VL53L5CX 8x8 ToF sensor** for wall-plane and per-pixel distance correction
- provides DISABLED / SHADOW / ACTIVE correction modes with fail-open behavior
- exposes a lightweight embedded Web UI for normal setup and diagnostics
- exposes power, brightness, RGB, and a small manual effect set to **Home Assistant** through WLED-compatible discovery/API
- stores runtime settings in NVS
- opens a recovery Wi-Fi AP after 60 seconds without a station connection
- reserves two 7 MiB application slots in a 16 MiB flash layout for future OTA work

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
   +--> brightness / manual owner arbitration
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

## HyperHDR

Configure the ESP32 as a **DDP** LED device:

```text
Protocol: DDP
Port:     UDP/4048
LEDs:     sum of the four active side counts
```

The active RGB payload is always `totalLedCount * 3` bytes.

The firmware holds the last complete DDP frame through short transport gaps. Partial or missing packets are never converted into an artificial black frame. A real complete black frame from HyperHDR still renders as black.

Do **not** configure this firmware as a WLED realtime device or HyperHDR Hyperk device.

## Home Assistant

The controller advertises `_wled._tcp.local.` and implements the subset of WLED JSON needed for the Home Assistant WLED integration.

The light entity exposes:

- on/off
- brightness
- RGB color
- `Ambilight`
- `Solid`
- `Rainbow`
- `Breathing`

Selecting `Ambilight` returns visible output ownership to DDP. Manual effects are local firmware modes. DDP reception continues in the background, so returning to Ambilight does not require restarting HyperHDR.

This is a compatibility facade, not a WLED fork. Unsupported WLED features are documented in [docs/wled-ha-compat.md](docs/wled-ha-compat.md).

## Web UI

Open:

```text
http://<controller-ip>/
```

The UI includes:

- **Home** - power, brightness, correction state, DDP and ToF summary
- **LED** - side topology, GPIO identification, direction/range tests, disabled-pixel mask
- **ToF** - 8x8 live matrix, geometry, gain curve, calibration
- **Diagnostics** - transport, heap, ToF, and renderer counters
- **System** - Wi-Fi provisioning and guarded factory reset

The UI is intentionally lightweight: no external assets, no WebSocket, and no web framework.

There is currently no Web UI authentication. Treat it as a trusted-LAN interface and do not publish TCP/80 to the internet.

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
- one physical LED per side can be masked black without shifting neighboring logical addresses
- the ToF page shows the normalized 8x8 grid relative to the TV

Commissioning patterns are brightness-limited in firmware.

See [docs/led-commissioning-patterns.md](docs/led-commissioning-patterns.md) and [docs/runtime-led-mapping.md](docs/runtime-led-mapping.md).

## Current validation

The current development line is validated with:

- partition check: PASS
- embedded Web UI structural check: PASS
- native tests: **267 / 267 PASS**
- ESP32-C6 build: PASS
- RAM: **95,460 / 327,680 bytes (29.1%)**
- application image: **1,338,858 / 7,340,032 bytes (18.2%)**

Exact numbers can move between commits. CI and `tools/validate.*` are the source of truth.

## Documentation

Start here:

- [Architecture](docs/architecture.md)
- [Local validation](docs/local-validation.md)
- [Runtime Wi-Fi](docs/runtime-wifi.md)
- [Web UI](docs/web-ui.md)
- [WLED / Home Assistant compatibility](docs/wled-ha-compat.md)
- [Runtime LED mapping](docs/runtime-led-mapping.md)
- [Runtime configuration recovery](docs/runtime-config-recovery.md)
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
