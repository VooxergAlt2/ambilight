# Vendored LiteLED

This directory is a controlled in-repository fork of LiteLED v3.2.0 from Xylopyrographer/LiteLED.

It is vendored intentionally for the Ambilight ESP32-C6 PARLIO hot path so performance-critical driver changes are reproducible and are never applied to PlatformIO's transient `.pio/libdeps` directory.

Upstream baseline: v3.2.0.

Ambilight divergence:
- fixed byte-wide PARLIO group contract at 8 lanes, matching the configured `data_width`;
- split group output into explicit `encode() / transmit() / wait()` stages with DMA ownership guards;
- replaced the lane-major clear-and-RMW group encoder with a bit-plane encoder that initializes constant waveform samples once;
- exposed a controlled lane pixel-buffer view so Ambilight can generate complete frames without per-pixel LiteLED API overhead.

The upstream MIT license is preserved in `LICENSE`.
