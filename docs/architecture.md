# Stage 7 architecture

## Scope

Stage 7 keeps the existing one-PC Wi-Fi/DDP renderer unchanged and adds raw VL53L5CX acquisition only.

No ToF-derived brightness correction exists yet.

USB/AWA is preserved on a separate WIP branch and is not part of this active line.

## Scheduling

ESP32-C6 is single-core, so the VL53L5CX must not become a blocking dependency of the renderer.

The application therefore has two independent paths.

Realtime frame path:

    Wi-Fi/lwIP
      -> DdpUdpService
      -> DdpAssembler
      -> FrameMailbox
      -> LedRenderer
      -> PARLIO x4

Sensor path:

    low-priority FreeRTOS task
      -> I2C
      -> Adafruit VL53L5CX / ST ULD
      -> 8x8 results
      -> TofSnapshot

Renderer never calls I2C and never waits for the sensor.

## Initialization

VL53L5CX initialization uploads sensor firmware and can take seconds.

Order:

1. FrameMailbox
2. PARLIO LED engine
3. Wi-Fi
4. DDP socket
5. ToF background task

Therefore sensor startup cannot delay DDP availability.

If sensor init fails, the task waits 5 seconds and retries while Ambilight continues normally.

## I2C

Development mapping:

- GPIO6 SDA
- GPIO7 SCL
- INT unused

Initial I2C frequency is 1 MHz to reduce firmware-upload and result-read occupancy.

This is explicitly subject to hardware acceptance. If the actual module/cable/pull-ups are marginal, repeat at 400 kHz.

## Snapshot contract

TofSnapshot contains:

- state
- generation/timestamp
- 64 raw distance values
- 64 raw target statuses
- valid-zone count
- diagnostic median
- init/read failure counters
- last/max sensor read duration

Only the ToF task touches the Adafruit/ST driver.

Main receives a copied snapshot through a short task mutex.

## Logging policy

A full 8x8 map is intentionally not streamed periodically because serial logging can distort DDP timing.

Periodic status includes only compact ToF metrics.

A one-shot raw map is printed only after the operator sends `t` over debug serial.

## Stage boundary

Stage 7 must not change RGB values.

The next ToF stage is allowed to analyze captured maps and build filtering/geometry logic, but brightness correction remains a later integration step.
