# Architecture

## Current scope

Stage 3 adds Wi-Fi station operation while generated test frames continue to drive the existing frame core and PARLIO renderer.

There is still no DDP/UDP payload path. This separation exists specifically to detect Wi-Fi scheduling or power-save side effects before the network becomes a frame source.

Still out of scope:

- DDP
- USB/AWA
- VL53L5CX
- Web UI
- OTA
- source arbitration
- multi-PC logic

## Logical frame contract

Every future input transport must publish one complete logical frame:

    RgbFrame
      generation
      receivedUs
      pixels[780] as RGB888

The RGB payload is exactly 2340 bytes.

Transport code is not allowed to address PARLIO lanes directly.

## Wi-Fi policy

ESP32-C6 Wi-Fi is configured as:

- station mode only
- persistent credential writes disabled
- auto reconnect enabled
- modem power-save disabled
- explicit ESP-IDF WIFI_PS_NONE

Power-save is deliberately disabled because future DDP traffic is realtime and jitter matters more than a small reduction in MCU power.

Credentials are not committed. Copy include/secrets.example.h to include/secrets.h and edit locally.

If credentials are absent, firmware still builds and Stage 3 runs with Wi-Fi disabled.

## Reconnect behavior

WifiService is non-blocking from the application perspective:

- WiFi.begin() starts the connection
- system Wi-Fi tasks handle association
- application tick observes state
- a reconnect request is issued at most once per 5 seconds while disconnected
- status metrics are printed every 10 seconds

No loop waits for WL_CONNECTED.

This is important because LED rendering must stay available even if the AP is unavailable.

## Frame path

Generated Stage 3 frames use the same future production path:

    test RgbFrame
         |
         v
    FrameMailbox
         |
         v
    LedRenderer
         |
         v
    SegmentMapper
         |
         v
      LedEngine
         |
         v
    LiteLED PARLIO x4

WifiService runs beside this path and is never called by LedRenderer.

## Stage 3 acceptance

With real Wi-Fi credentials:

- connect without blocking LED startup
- WIFI_PS_NONE active
- 60 FPS probe remains stable
- mappingErrors remains zero
- no significant regression in max PARLIO show time
- AP/router restart recovers without rebooting C6
- LED test patterns continue while disconnected

## Next stage

Stage 4 adds a pure C++ DDP parser and reassembler with host-side tests. It still does not connect UDP packets directly to PARLIO.
