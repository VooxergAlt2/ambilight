# Architecture

## Current stage

Stage 9 keeps Wi-Fi/DDP as the only active RGB transport and adds a fully testable, diagnostic-only ToF gain model.

Active RGB path:

    HyperHDR
      -> Wi-Fi / DDP
      -> DdpUdpService
      -> DdpAssembler
      -> FrameMailbox
      -> LedRenderer
      -> SegmentMapper
      -> PARLIO x4

Independent ToF path:

    VL53L5CX 8x8 @ 10 Hz
      -> TofService
      -> TofRawFrame
      -> TofProcessor
      -> TofGeometrySnapshot
      -> TofGainModel
      -> GainSnapshot

There is still no edge from GainSnapshot to LedRenderer.

## Layer boundaries

### DDP transport

Owns packet receive/reassembly only.

It cannot see ToF.

### TofProcessor

Pure C++ spatial/temporal geometry processor.

It cannot see RGB or LED hardware.

### TofGainModel

Pure C++ calibration/fail-open layer.

It converts stable geometry into future side attenuation coefficients.

It cannot see RGB or LED hardware.

### LedRenderer

Still consumes only RgbFrame.

This prevents calibration bugs from changing live Ambilight before hardware calibration is complete.

## Gain contract

GainSnapshot contains:

- leftQ12
- rightQ12
- topQ12
- bottomQ12
- geometryUsable
- failOpen
- generation/timestamp

Unity is Q12 4096.

Gain values above unity are not supported.

## Current calibration

Production/default curve is intentionally identity:

    50..4000 mm -> 4096

No brightness change is possible from Stage 9 model output.

## Fail-open

The gain model produces unity when geometry is invalid or older than 1.5 seconds.

The ToF task refreshes stale state even when the sensor stops producing frames, so a future renderer integration will not indefinitely hold old attenuation.

## Fault isolation

VL53L5CX failures can:

- invalidate geometry
- force future gains to unity
- restart the ToF sensor task

They cannot:

- restart Wi-Fi
- restart DDP
- clear valid RGB transport state
- reboot MCU
- call LedRenderer

## Debug commands

- `t`: raw 8x8 ToF
- `g`: processed geometry
- `k`: diagnostic future gains

## Next gate

Actual brightness integration is blocked on real hardware calibration captures.

The code may proceed to renderer integration only after control points are chosen from measured behavior rather than guessed.
