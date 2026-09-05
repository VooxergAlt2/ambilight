# USB/AWA WIP checkpoint

This branch preserves the USB/AWA work started after the Wi-Fi/DDP hardening stage.

It is intentionally **not** part of the active product path yet.

Current active development remains:

    HyperHDR
      -> Wi-Fi / DDP
      -> ESP32-C6
      -> FrameMailbox
      -> LedRenderer
      -> PARLIO x4

## Preserved USB work

The WIP contains:

- a clean C++ AWA parser for 780 RGB LEDs
- support for normal `Awa` RGB frames
- support for `AwA` frames with four calibration bytes included in Fletcher checksums
- HyperHDR ESP handshake request detection
- HyperHDR sleep request detection
- Fletcher1/Fletcher2/FletcherExt validation
- strict full-frame publication semantics
- direct ESP-IDF `usb_serial_jtag` transport skeleton
- fixed RX/TX ring buffers
- bounded USB polling and backlog collapse
- handshake response compatible with HyperSerial v11
- parser unit tests

## Important design decisions preserved

- native USB Serial/JTAG is reserved for HyperHDR data, not debug logging
- Arduino HWCDC is not intended as the final high-throughput RX path
- USB and DDP must eventually publish the same `RgbFrame` contract
- no partial AWA frame may reach `FrameMailbox`
- intermediate complete frames may be collapsed under backlog
- USB/AWA will be reintroduced only after the Wi-Fi/DDP + ToF path is stable

## Not finished yet

Before this branch is promoted, it still needs:

- final PlatformIO flags to prevent Arduino HWCDC ownership conflicts
- USB-only runtime `main.cpp`
- actual ESP32-C6 throughput benchmark
- compile verification against the pinned Arduino/IDF toolchain
- HyperHDR device-detection validation on Windows
- physical USB-switch testing
- reconnect / COM re-enumeration behavior
- long-run AWA stress test
- later source arbitration with DDP

Do not merge this branch into the active Wi-Fi line yet.
