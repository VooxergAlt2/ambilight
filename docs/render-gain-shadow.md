# Stage 11: ToF render shadow integration

## Purpose

Stage 11 connects the ToF gain pipeline to the render pipeline for the first time, but only in shadow mode.

The firmware computes exactly what ToF correction would do to each logical LED while guaranteeing that physical output remains the original HyperHDR RGB.

## Active frame path

    HyperHDR
      -> Wi-Fi / DDP
      -> RgbFrame
      -> frame-boundary GainSnapshot copy
      -> TofRenderGainBridge
      -> RenderGainContext
      -> LedRenderer shadow math
      -> ShadowRenderPolicy
      -> ORIGINAL RGB
      -> SegmentMapper
      -> PARLIO x4

The shadow candidate is never sent to LedEngine.

## Why the bridge exists

LedRenderer does not depend on VL53L5CX or TofService types.

ToF-specific:

    GainSnapshot

is translated in main/integration code into the renderer-neutral:

    RenderGainContext

This keeps render code reusable if future gains come from:

- manual calibration
- a different distance sensor
- a plane-fit model
- a debug override
- USB-era source arbitration

## Frame atomicity

ToF runs around 10 Hz while RGB render runs around 60 Hz.

Before each new RGB frame is rendered:

1. copy one small GainSnapshot under the ToF mutex
2. release the mutex
3. validate freshness/fail-open
4. create one immutable RenderGainContext
5. use that same context for all 780 logical pixels

A ToF update can therefore never change gains halfway through one LED frame.

## Defense-in-depth freshness

TofGainModel already fails open when geometry becomes stale.

TofRenderGainBridge performs an independent renderer-side freshness check:

    max GainSnapshot age = 1.5 s

If:

- snapshot is absent
- timestamp is invalid/future
- snapshot is stale
- geometryUsable is false
- failOpen is true

then RenderGainContext exposes unity gain.

This protects future rendering even if the ToF task itself stalls completely.

## Q12 gain math

Gain is applied identically to R, G and B:

    out = round(channel * gainQ12 / 4096)

with:

    4096 = 1.000
    2048 = 0.500
    0    = 0.000

Gain is clamped at unity.

This preserves RGB channel ratios and is the correct layer for per-side optical attenuation.

Do not implement ToF correction through:

- HSV value manipulation
- power PWM of a strip segment
- LiteLED global brightness
- HyperHDR-side global brightness

The existing LiteLED global test brightness remains a separate final multiplier.

## Logical gain profile

RenderGainContext does not store only one scalar per side.

Each SegmentId has:

    startQ12
    endQ12

The renderer linearly interpolates gain in LOGICAL segment order.

Current TofRenderGainBridge sets:

    startQ12 == endQ12

for every segment because the current Stage 9 model still emits one gain per side.

The endpoint interface is intentional future-proofing.

For a yawed TV, TOP and BOTTOM eventually need a left-to-right gain gradient. Plane-fit or endpoint geometry can provide this later without changing LedRenderer.

## Logical versus physical direction

Gain interpolation happens before physical lane reversal.

SegmentMapper returns:

- SegmentId
- logical segment offset
- logical segment length
- physical lane/index

Therefore reversing a strip for wiring cannot accidentally reverse the mathematical gain profile.

Before a non-uniform gradient is ever activated, the physical screen-space meaning of logical start/end for every segment must be explicitly verified.

## ShadowRenderPolicy

Stage 11 has no runtime enable switch for correction.

The policy is compile-time:

    physicalOutput(original, shadowCandidate) = original

Native tests explicitly assert that even a strongly modified shadow candidate cannot reach physical output.

A future activation stage must deliberately replace this policy and its tests.

## Shadow diagnostics

For every successfully rendered frame LedRenderer records:

- source-present frames
- source-usable frames
- fail-open frames
- non-unity-context frames
- evaluated pixels
- pixels that would change
- pixels that would change per segment
- maximum channel delta
- current frame original RGB sum
- current frame shadow RGB sum
- source generation/age
- CPU preparation time before PARLIO
- maximum preparation time

Serial command:

    r

prints the complete shadow state and current segment start/end gains.

## Performance observation

The shadow path intentionally executes future per-pixel gain math now.

That means Stage 11 can reveal CPU cost before gains are activated.

Important metrics:

- DDP p50/p95/p99 internal frame age
- renderer prepare time
- PARLIO show time
- backlog skips

No hard prepare-time threshold is declared until first ESP32-C6 hardware measurements are collected.

## Current expected result

With the shipping identity calibration:

    50 mm   -> 100%
    4000 mm -> 100%

a healthy sensor should produce:

- sourcePresent = yes
- sourceUsable = yes
- failOpen = no
- nonUnity = no
- wouldChangePixels = 0
- shadow/original RGB = 100.0%

This still exercises snapshot copying, bridge validation, segment lookup, interpolation and Q12 math.

## Future activation sequence

Do not jump directly from Stage 11 to active attenuation.

Recommended sequence:

1. collect real calibration captures
2. install real gain curve
3. keep shadow mode
4. observe non-unity shadow metrics on real HyperHDR content
5. verify segment orientation
6. verify top/bottom gradient model if used
7. compare DDP latency before/after shadow math
8. only then create a separate PR that changes physical output policy

This keeps the dangerous one-line change isolated and reviewable.
