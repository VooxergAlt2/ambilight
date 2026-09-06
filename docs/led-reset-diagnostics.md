# LED reset diagnostic run

This branch isolates ESP32-C6 resets that occur when a commissioning LED test
starts. It is intentionally separate from the validated Stage 39 line.

Firmware identity:

    0.39.0-led-reset-diag

## Diagnostic changes

- Arduino loop-task stack raised from the default 8 KiB to 16 KiB.
- Commissioning uses a persistent global unity gain context instead of creating
  a roughly 1.9 KiB RenderGainContext temporary on the loop-task stack.
- Boot prints esp_reset_reason().
- The first commissioning output prints loop stack high-water data plus free and
  largest internal DMA-capable heap.
- Breadcrumbs bracket logical renderer output and raw PARLIO show().
- CORE_DEBUG_LEVEL=4 exposes LiteLED/PARLIO debug logs.

No DDP, ToF, topology, mapping, pixel-mask or persistence contract is changed.

## Test sequence

Keep the serial monitor open from boot and use output brightness 1.

Run one command at a time:

    b1
    jgpio:18:0:1
    jgpio:19:0:1
    jgpio:20:0:1
    jgpio:21:0:1
    jside:0:0:1
    i1

After any reset, capture the complete boot output and especially:

    BOOT DIAG reset_reason=...
    LED DIAG tag=commissioning-before-output ...
    LED DIAG breadcrumb=...
    LED DIAG tag=commissioning-after-output ...

## Interpretation

If a raw GPIO test resets before:

    raw-after-parlio-show

the fault is below LedRenderer: LedEngine / LiteLED / PARLIO / GPIO / electrical
integrity.

If raw GPIO succeeds but logical side resets after:

    logical-before-render

and before:

    logical-after-render

the fault is in the logical renderer path or its stack/call depth.

If one-pixel logical tests work but the full segment pattern resets, the
renderer call path itself is viable and the distinguishing factor is the
non-zero frame contents / physical output pattern.

Reset reason is authoritative for the broad fault class:

- PANIC: exception/assert/stack corruption/software panic.
- INT_WDT/TASK_WDT/WDT: watchdog.
- BROWNOUT: supply voltage fell below the brownout threshold.
- PWR_GLITCH: hardware power-glitch detector.
- SW: software restart.
- CPU_LOCKUP: double exception/CPU lockup.

The branch should not be merged into Stage 39 until the hardware root cause is
identified and diagnostic-only logging is removed or deliberately retained.
