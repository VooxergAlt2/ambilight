# Runtime Wi-Fi provisioning

## Credential priority

Startup resolves Wi-Fi credentials in this order:

1. NVS credentials
2. compile-time fallback from `secrets.h`
3. no credentials -> wait 60 seconds, then open the fallback AP

The saved station password is never printed by firmware diagnostics. The
fixed fallback-AP credential is intentionally documented and may be printed so
the recovery network remains usable without stored secrets.

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

## Fallback access point

If the station interface has not connected for 60 seconds, including after a
later prolonged disconnect, firmware opens a provisioning AP while continuing
STA reconnect attempts:

    SSID:     Ambilight-XXXXXX
    password: ambilight
    IP:       4.3.2.1

`XXXXXX` is derived from the controller MAC/chip identity so nearby Ambilight
controllers do not all advertise the same SSID. HTTP/80 and DDP UDP/4048 bind
to the fallback network as well, so the existing Web UI can be used to enter
new Wi-Fi credentials at `http://4.3.2.1/`.

The AP is not a replacement for STA. A successful station connection closes
the fallback AP automatically. If the AP itself cannot be started, firmware
retries AP startup every 5 seconds rather than waiting another full minute.

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
    DDP socket stopped
    station disabled
    fallback AP scheduled after 60 s

If credentials are cleared while already connected through the fallback AP,
the AP remains up so the provisioning page does not deliberately disconnect
the operator.

If compile-time fallback configuration is invalid, firmware does not silently
continue with old credentials; it schedules the same fallback AP path.

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
