# Architecture

## Current stage

Stage 13 adds independent render dirtiness for RGB and gain state.

Physical gain application remains disabled.

## Sources

RGB:

    HyperHDR -> Wi-Fi/DDP -> FrameMailbox -> cached RgbFrame

ToF:

    VL53L5CX
      -> TofProcessor
      -> TofGainModel
      -> cached GainSnapshot
      -> TofRenderGainBridge
      -> target RenderGainContext

## Render state

The application now owns:

- cached RGB frame
- RGB dirty flag
- cached target gain context
- RenderGainController
- RenderScheduler

## Decision model

Every loop:

1. drain DDP
2. update cached RGB if mailbox generation changed
3. refresh cached gain target at <=100 Hz
4. compute:
   - target profile changed?
   - gain controller unsettled?
5. scheduler decides render/no-render
6. if rendering:
   - new RGB updates DDP latency metrics
   - gain-only does not
   - controller advances effective gains
   - LedRenderer evaluates shadow
   - ShadowRenderPolicy outputs original RGB

## Rate limits

RGB frames:
- immediate

Gain-only:
- <= about 60 Hz

ToF target polling:
- <=100 Hz

Sensor ranging:
- about 10 Hz

## Snapshot failure behavior

A transient failed GainSnapshot mutex copy keeps the previous cached snapshot.

Staleness is determined from the snapshot timestamp by TofRenderGainBridge.

This distinguishes:

- temporary synchronization miss
- genuine stale sensor data

## Dirty-profile semantics

Render profile equality ignores diagnostic metadata.

It compares:

- usable/fail-open state
- effective segment gain endpoints

This minimizes unnecessary rerenders.

## Static image behavior

A cached RGB frame may be rendered multiple times with evolving shadow gains even if no new DDP frame arrives.

This is required for environmental correction to work independently from source-frame cadence.

## Safety

Stage 13 still cannot apply gains physically.

The final hardware color remains original HyperHDR RGB.
