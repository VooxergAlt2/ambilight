# Architecture

## Current stage

Stage 11 introduces the first connection between ToF-derived gains and rendering, but the connection is shadow-only.

RGB source remains Wi-Fi/DDP only.

## Data paths

### RGB

    HyperHDR
      -> Wi-Fi/DDP
      -> DdpAssembler
      -> FrameMailbox
      -> RgbFrame

### ToF

    VL53L5CX
      -> TofProcessor
      -> TofGeometrySnapshot
      -> TofGainModel
      -> GainSnapshot

### Integration

At one RGB frame boundary:

    GainSnapshot
      -> TofRenderGainBridge
      -> RenderGainContext

Then:

    RgbFrame + RenderGainContext
      -> LedRenderer
      -> shadow candidate RGB
      -> ShadowRenderPolicy
      -> original RGB
      -> LedEngine
      -> PARLIO x4

## Dependency rule

LedRenderer knows only:

- RgbFrame
- RenderGainContext
- SegmentMapper
- LedEngine

It does not know:

- VL53L5CX
- TofService
- TofProcessor
- TofGainModel

The only ToF/render coupling is TofRenderGainBridge at the application integration layer.

## Context semantics

RenderGainContext is immutable for one render call.

It contains:

- source generation
- source timestamp/age
- source present
- source usable
- fail-open
- start/end Q12 gain for each SegmentId

## Segment endpoint model

Each segment has logical start/end gain.

This supports:

- uniform side gain now
- future linear top/bottom gradients
- wiring reversal without profile reversal

Physical lane reversal remains SegmentMapper responsibility.

## Hardware safety invariant

Stage 11 physical RGB is defined by:

    ShadowRenderPolicy::physicalOutput(original, candidate)

which returns:

    original

There is no runtime flag to change this.

## Failure policy

Absent, stale or invalid gain data creates a unity RenderGainContext.

ToF failure cannot:

- change RGB
- stop DDP
- restart Wi-Fi
- restart PARLIO
- hold stale attenuation indefinitely

## Runtime observability

Command set:

- t: raw ToF grid
- g: processed ToF geometry
- k: ToF GainSnapshot
- c: 5-second calibration capture
- r: renderer shadow diagnostics

## Current activation gate

Physical gain application remains prohibited until:

- real calibration curve exists
- logical segment screen orientation is verified
- shadow mode demonstrates correct per-segment behavior
- render preparation cost is acceptable
- DDP latency does not regress materially
