# Windows: HyperHDR integration and firmware installation

This project does not require a custom Windows plugin or a vendor-specific PC
service. The PC-side component is **HyperHDR**, and the controller is configured
there as a standard **DDP** network LED device.

That separation is deliberate:

- HyperHDR captures the screen and produces realtime RGB frames;
- HyperHDR sends those frames to the ESP32-C6 over **DDP UDP/4048**;
- the ESP32-C6 owns LED topology, physical output, optional ToF correction,
  commissioning, brightness and the disabled-pixel mask;
- Home Assistant uses the controller's separate WLED-compatible HTTP facade;
- no WLED realtime UDP transport and no HyperHDR Hyperk emulation are used.

## HyperHDR setup on Windows

1. Install HyperHDR for Windows from the official HyperHDR project.
2. Put the PC and the ESP32-C6 controller on the same LAN.
3. Open the controller Web UI and note its IP address and total active LED
   count.
4. In HyperHDR, add/configure a network LED output using **DDP**.
5. Set the target address to the controller IP.
6. Use UDP port **4048**.
7. Set the LED count to the sum of the four configured TV sides shown in the
   controller Web UI.
8. Keep realtime output on DDP. Do not select WLED realtime UDP or Hyperk for
   this firmware.

The firmware has no separate 920-style aggregate LED-count ceiling. Each of the four side
lengths is stored as a 16-bit value (up to 262,140 logical LEDs total by format), and runtime RGB/DDP/PARLIO storage scales
with the configured topology until controller resources are exhausted. Hardware
testing has been performed up to **230 LEDs on one physical output**; larger
configurations are currently experimental on real hardware.

The controller accepts one active DDP sender lease at a time. If two PCs send
DDP concurrently, only one sender owns the stream until the short lease expires.
This prevents packets from multiple HyperHDR instances from being mixed into a
single frame.

HyperHDR LED order must match the logical perimeter order configured by the
controller. Physical GPIO assignment and FWD/REV direction are handled on the
ESP32 side, so correct wiring mistakes in the controller Web UI rather than by
inventing gaps or duplicate LEDs in HyperHDR.

If DDP packets stop briefly, the firmware keeps the last complete frame. It does
not synthesize a black frame from a transport gap. A complete black frame sent
by HyperHDR is still authoritative and turns the LEDs black.

## Firmware installation on Windows

The release contains two user-facing firmware images:

- `firmware.factory.bin` - complete first-install image for a blank controller;
- `firmware.bin` - application-only image for updating an already configured
  controller while preserving the NVS settings partition.

The release may also include `bootloader.bin` and `partitions.bin` for advanced
manual recovery/debugging. Normal installation does not require them.

### Requirements

- Python 3 for Windows;
- a USB data cable;
- the COM port of the ESP32-C6 board;
- `esptool` installed with:

```powershell
py -m pip install --upgrade esptool
```

If the board uses an external USB-to-UART bridge, Windows may also need that
bridge's driver. ESP32-C6 boards using native USB generally enumerate without a
separate vendor driver on current Windows versions.

### First installation

For a blank board, download `firmware.factory.bin`, replace `COM5` with the
actual port, and run:

```powershell
py -m esptool --chip esp32c6 --port COM5 write-flash 0x0 firmware.factory.bin
```

After reboot, if no saved Wi-Fi network connects within 60 seconds, the
controller opens its recovery AP:

```text
SSID:     Ambilight-XXXXXX
password: ambilight
Web UI:   http://4.3.2.1/
```

Use the Web UI to configure Wi-Fi and LED topology.

### Update without losing settings

Before updating, downloading a JSON backup from **System -> Configuration
backup** is recommended.

For the current partition layout, update an already configured controller with
`firmware.bin` at application offset `0x10000`:

```powershell
py -m esptool --chip esp32c6 --port COM5 write-flash 0x10000 firmware.bin
```

Do **not** run `erase-flash` before a normal update. Do **not** use
`firmware.factory.bin` as an update image if the existing NVS settings must be
kept. Both a full-chip erase and writing the combined factory image across the
early flash region can destroy saved configuration.

The `firmware.bin` update method assumes the release's committed partition
layout is already installed. If the partition table ever changes incompatibly,
the release notes will call that out explicitly and a backup/re-provisioning
path may be required.

## Home Assistant nuance

Home Assistant discovery is intentionally independent from HyperHDR. The
controller advertises a limited WLED-compatible service for power, brightness,
RGB and the local `Ambilight`, `Solid`, `Rainbow` and `Breathing` modes.

Selecting a local/manual effect temporarily gives visible LED ownership to the
controller. Selecting `Ambilight` hands visible output back to the newest DDP
frame. DDP reception continues in the background while a manual effect is
active.

## Network and security notes

- DDP uses UDP/4048.
- The Web UI and WLED-compatible API use HTTP/80.
- The Web UI currently has no separate authentication. Keep it on a trusted
  LAN and do not expose TCP/80 directly to the internet.
- Wi-Fi passwords are stored locally in NVS but are intentionally excluded from
  JSON configuration backups.
