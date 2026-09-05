# Architecture

## Current stage

Stage 12 adds a render-rate gain controller to the shadow-only ToF integration.

Physical RGB remains original HyperHDR RGB.

## RGB path

    HyperHDR
      -> Wi-Fi/DDP
      -> RgbFrame
      -> FrameMailbox

At each new RGB frame:

    GainSnapshot
      -> TofRenderGainBridge
      -> target RenderGainContext
      -> optional debug ShadowGainProbe target
      -> RenderGainController
      -> effective RenderGainContext

Then:

    RgbFrame + effective RenderGainContext
      -> LedRenderer shadow math
      -> ShadowRenderPolicy
      -> ORIGINAL RGB
      -> LedEngine
      -> PARLIO x4

## Rate domains

ToF:

    ~10 Hz

RGB:

    ~60 Hz

The controller is the boundary between those rate domains.

It prevents future active brightness from inheriting 10 Hz stair-step behavior.

## State ownership

TofService owns:

- sensor
- geometry
- GainSnapshot

Main/integration owns:

- target context selection
- debug shadow probe
- RenderGainController

LedRenderer owns:

- per-pixel shadow evaluation
- render diagnostics

LedEngine owns:

- physical lanes
- PARLIO output

## Fail-open

Renderer-side bridge validates GainSnapshot freshness.

RenderGainController treats unusable/fail-open targets as an immediate unity command.

Therefore stale attenuation cannot survive either:

- ToF model failure
- sensor-task stall
- integration-layer stale snapshot

## Gradient readiness

RenderGainContext carries logical start/end gains per SegmentId.

RenderGainController slews both endpoints independently.

LedRenderer interpolates them per logical pixel.

Current real ToF bridge still produces uniform endpoints.

ShadowGainProbe exercises non-uniform endpoints before a physical wall-plane model exists.

## Safety invariant

Physical output remains compile-time shadow-only.

No Stage 12 code path can select shadow candidate RGB for LedEngine.

## Debug commands

- t: raw ToF map
- g: processed geometry
- k: gain snapshot
- c: calibration capture
- r: target/effective render shadow state
- x: 10-second aggressive shadow probe

## Next gate

Use x + r on hardware to validate the renderer mechanics independently from ToF calibration.

Then derive real calibration values and keep them in shadow mode before considering physical application.
