# Render correction pipeline

## Purpose

The render pipeline separates:

1. candidate correction math
2. physical output policy

This allows the same geometry/gain implementation to operate in DISABLED, SHADOW and ACTIVE modes.

## Path

    HyperHDR RGB
      -> RenderGainContext
      -> RenderGainController
      -> RenderGainMath::preview
      -> candidate RGB
      -> CorrectionOutputPolicy
           DISABLED -> original RGB
           SHADOW   -> original RGB
           ACTIVE   -> candidate RGB
      -> SegmentMapper
      -> LedEngine
      -> PARLIO x4

## Renderer-neutral bridge

ToF-specific snapshots are translated by TofRenderGainBridge into RenderGainContext.

LedRenderer does not depend on VL53L5CX or TofService.

The exact active logical gain field is applied before physical lane reversal.

## Candidate math

For every channel:

    out = round(channel * gainQ12 / 4096)

where:

    4096 = 1.0
    2048 = 0.5

Gain is clamped at unity and is identical for R/G/B, preserving channel ratios.

## Runtime modes

See:

    docs/correction-modes.md

SHADOW remains the default.

ACTIVE is an explicit persisted user choice.

## Fail-open

RenderGainContext returns unity whenever the source is unusable.

Therefore even ACTIVE mode physically outputs original RGB when ToF geometry becomes invalid or stale.

Fail-open snaps to unity immediately rather than slewing stale attenuation away.

## Mode transition

Entering ACTIVE resets effective gains to unity before slewing to the current valid target.

This prevents a previously accumulated SHADOW profile from becoming visible in one abrupt frame.

Leaving ACTIVE restores original RGB immediately.

## Diagnostics

The `r` command reports both candidate and physical effects:

- correction mode
- total DISABLED / SHADOW / ACTIVE frames
- candidate-changed pixels
- physically changed pixels
- source usability
- candidate RGB ratio
- target/effective gains
- gain slew
- scheduler activity

Periodic STAT includes current mode and last candidate/physical changed-pixel counts.

## Synthetic probe

The `x` profile is diagnostic only.

It is allowed exclusively in SHADOW mode and is cancelled by every mode transition.

## Default gain curve

The repository still ships an identity ToF curve, so ACTIVE currently has no ToF-driven visual attenuation until calibration coefficients are configured.

That is a calibration-data limitation, not an architectural limitation.
