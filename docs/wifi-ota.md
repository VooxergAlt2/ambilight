# Wi-Fi OTA

Stage 47 adds a native browser OTA path for already configured ESP32-C6 controllers. It is intentionally separate from the WLED compatibility facade and is designed for a trusted LAN.

## Update flow

1. Open `http://<controller-ip>/` and go to **System → Wi-Fi firmware update**.
2. Press **Arm OTA for 120 s**. The normal HTTP/80 control path creates a random one-time token.
3. Choose **`firmware.bin`** from the release.
4. Start upload. The browser streams a multipart upload to `http://<controller-ip>:3232/update?token=...`.
5. The controller blacks LED output, validates the image, writes the inactive app slot, finalizes the OTA partition, replies, then reboots after about one second.

NVS is a separate partition and is preserved. After reboot the existing Wi-Fi, topology, correction, split brightness and local-lighting settings remain available.

## Accepted image

Only the normal ESP32-C6 **application image** is valid for OTA. Before flash writing starts the service checks:

- ESP image magic;
- sane segment count;
- ESP32-C6 chip ID;
- first-segment size sufficient for `esp_app_desc_t`;
- application descriptor magic at the first-segment start.

The generated `firmware.factory.bin` and `bootloader.bin` fail this check. The combined factory image remains only for blank-device installation/recovery from flash offset 0.

## Failure behavior

OTA is locked by default. An arm token expires after 120 seconds and is consumed as soon as an upload begins. A retry requires a fresh arm. Upload data is streamed in WebServer chunks rather than buffered as a complete firmware image.

While writing flash, DDP rendering and local effects are suspended and physical brightness is forced to zero. If validation/write/finalization fails, the update is aborted and the previous selected source and brightness are restored. If finalization succeeds, output remains black until reboot.

The implementation does **not** claim authenticated/TLS Internet OTA. TCP/80 and TCP/3232 are trusted-LAN surfaces and should not be exposed to the public Internet.

## Recovery

If network OTA is unavailable, use the documented USB/esptool method. For a configured controller, `firmware.bin` at `0x10000` preserves NVS. A whole-chip erase or flashing `firmware.factory.bin` at offset 0 can overwrite/erase settings and should be treated as recovery/first-install operations.
