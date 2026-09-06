# Runtime Wi-Fi provisioning

## Credential priority

Startup resolves Wi-Fi credentials in this order:

1. NVS credentials
2. compile-time fallback from `secrets.h`
3. no credentials -> Wi-Fi/DDP/Web disabled

The password is never printed by firmware diagnostics.

## Serial commands

Show Wi-Fi status:

    w<Enter>

Set and immediately apply credentials:

    wSSID|PASSWORD<Enter>

Clear NVS credentials:

    wclear<Enter>

The first `|` separates SSID and password. A pipe character therefore cannot be used inside the SSID. The password may contain additional pipe characters.

Limits:

- SSID: 1..32 bytes
- password: 0..63 bytes

An empty password is allowed for an open network.

## Runtime behavior

Setting new credentials:

1. validates lengths
2. attempts to persist SSID/password in NVS
3. reconfigures ESP32 STA immediately
4. keeps Wi-Fi power-save disabled
5. starts DDP UDP/4048 if it was not already running
6. starts the minimal HTTP/80 listener if it was not already running

No reboot is required.

If NVS is unavailable, the credentials still work for the current boot and are reported as:

    RUNTIME_VOLATILE

## Sources

Runtime diagnostics report one of:

    NONE
    COMPILE_TIME
    NVS
    RUNTIME_VOLATILE

## Clearing credentials

`wclear` removes NVS credentials.

If compile-time fallback exists:

    NVS -> COMPILE_TIME

If no fallback exists:

    Web UI listener stopped
    Wi-Fi disabled
    DDP socket stopped

If fallback configuration is invalid, Wi-Fi/DDP/Web are explicitly disabled rather than silently continuing with old credentials.

The web UI never returns the saved password. Changing credentials from the
browser may move the controller to another network, so the current page can
become unreachable immediately after the acknowledged request is applied.

## Security note

NVS storage is convenient provisioning, not a substitute for flash encryption.

Firmware does not expose the saved password in serial output, but an attacker with physical flash access may still recover unencrypted NVS contents if platform security features are disabled.

## Compile-time fallback

`include/secrets.h` remains optional.

Example:

    #define AMBILIGHT_WIFI_SSID "your-ssid"
    #define AMBILIGHT_WIFI_PASSWORD "your-password"

This is now a fallback, not the primary configuration mechanism.
