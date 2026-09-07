# Vendored LiteLED

This directory is a controlled in-repository fork of LiteLED v3.2.0 from Xylopyrographer/LiteLED.

It is vendored intentionally for the Ambilight ESP32-C6 PARLIO hot path so performance-critical driver changes are reproducible and are never applied to PlatformIO's transient .pio/libdeps directory.

Upstream baseline: v3.2.0.
License: MIT, see LICENSE in this directory.
