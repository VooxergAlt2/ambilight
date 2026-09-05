# ESP32-C6 Ambilight Controller

Custom ESP32-C6 Ambilight endpoint for HyperHDR.

## Active development path

The active firmware path is currently **Wi-Fi/DDP only**.

Stage 7 keeps the proven one-PC DDP renderer and adds raw VL53L5CX acquisition in the background.

    HyperHDR
        |
        | Wi-Fi / DDP / UDP 4048
        v
    ESP32-C6
        |
        v
    DdpAssembler
        |
        v
    FrameMailbox
        |
        v
    LedRenderer
        |
        v
    PARLIO x4

In parallel:

    VL53L5CX 8x8 @ 10 Hz
        |
        | I2C
        v
    low-priority ToF task
        |
        v
    immutable TofSnapshot

The ToF snapshot does **not** modify LED brightness yet.

## Why ToF initialization is backgrounded

VL53L5CX uploads firmware to the sensor during initialization and can take several seconds.

The sensor therefore runs in a low-priority FreeRTOS task started only after the DDP/LED runtime is initialized.

Ambilight remains available while the sensor starts or while the sensor is absent.

## ToF development settings

Provisional pins:

- SDA GPIO6
- SCL GPIO7
- INT unused

Initial mode:

- 8x8 / 64 zones
- 10 Hz
- 1 MHz I2C
- target status 5 or 9 counted as valid for diagnostics

If the actual breakout/wiring is unstable at 1 MHz, hardware acceptance should repeat at 400 kHz.

## Raw map inspection

Normal logging stays compact to avoid injecting UART stalls into realtime DDP operation.

Send:

    t

to the debug serial terminal to print one raw 8x8 distance/status map.

## USB/AWA

USB/AWA work is preserved separately in branch:

    stage/07-usb-awa

It is intentionally not part of the active Wi-Fi firmware line yet.

## Build

Firmware:

    pio run -e esp32-c6-devkitc-1

Native tests:

    pio test -e native

## Stage 7 acceptance

- DDP 60 FPS behavior remains unchanged with ToF task enabled
- DDP starts before VL53L5CX initialization finishes
- missing VL53L5CX does not break Ambilight
- sensor reaches 8x8 @ 10 Hz when connected
- raw maps react plausibly to TV/wall position
- ToF read time and max read time are visible
- DDP p95/p99 internal frame age does not regress materially
- no progressive heap loss
- no watchdog/reset

Adaptive brightness comes only after real raw maps are captured and reviewed.
