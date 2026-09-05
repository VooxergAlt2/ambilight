# Stage 13: render dirty scheduler

## Purpose

Stage 13 removes the hidden assumption that LED rendering only needs to happen when a new RGB frame arrives.

Future ToF brightness correction must also be able to update a static image.

Physical output is still shadow-only and remains original HyperHDR RGB.

## Problem being solved

Previous stages rendered only when FrameMailbox published a new RgbFrame.

That is normally fine while HyperHDR continuously sends frames, but it is not a correct architectural contract for environmental gain correction.

Example:

1. HyperHDR shows a static image and stops retransmitting it.
2. TV bracket moves.
3. VL53L5CX geometry changes.
4. Gain target changes.
5. Without Stage 13 the old RGB frame would never be rerendered with the new gain state.

Stage 13 separates RGB dirty state from gain dirty state.

## New render causes

A render may now be requested by:

    new RGB frame

or:

    changed/unsettled gain profile

or both at once.

## Cached RGB frame

The latest complete RgbFrame is cached independently from its render status.

State:

- lastMailboxGeneration
- lastRenderedRgbGeneration
- rgbFrameValid
- rgbDirty

A newer mailbox frame replaces the cached frame and marks RGB dirty.

Gain-only rendering reuses the cached RGB without modifying its generation.

## Target gain cache

ToF GainSnapshot is not copied on every main-loop iteration.

Polling interval:

    10 ms

The last successful GainSnapshot is retained.

A temporary mutex-copy failure does not immediately fail open.

Instead:

1. keep the previous successful snapshot
2. pass its timestamp through TofRenderGainBridge
3. renderer-side freshness decides when it is truly stale

This prevents a brief mutex collision from causing a false unity jump.

## Dirty gain logic

Target context is considered changed when its effective render profile changes.

Metadata alone does not make rendering dirty:

- generation
- timestamp
- age

do not trigger a rerender when the actual gains are unchanged.

However:

    usable unity

and:

    fail-open unity

are considered different states.

This ensures sensor reacquisition/failure state is not silently ignored.

Gain dirty is true when:

    target profile changed

or:

    RenderGainController is not settled

## Render scheduler

Gain-only rerenders are capped to approximately 60 Hz:

    minimum start-to-start interval = 16667 us

Fresh RGB frames bypass this gain-only limiter and render immediately.

This avoids turning a fast main loop into a 100+ FPS PARLIO loop merely because gain slew is active.

## Why start-to-start timing

PARLIO show time is roughly 6.9 ms for the 230-pixel longest lane.

The gain-only limit is measured from render start rather than render end.

That preserves an approximately 60 Hz start cadence instead of adding 6.9 ms on top of every 16.7 ms interval.

## DDP latency isolation

Gain-only rerenders intentionally reuse an old RgbFrame.

They must not enter DDP frame-age statistics.

The network latency histogram updates only when the render decision includes a genuinely new RGB frame whose timestamp matches the latest completed DDP frame.

Therefore a 500 ms old static image rerendered due to ToF does not appear as 500 ms of network queue latency.

## Blackout

Idle blackout remains a normal mailbox publication.

It therefore becomes ordinary RGB dirty state and passes through the same scheduler.

No special renderer bypass exists.

## Scheduler diagnostics

Counts:

- RGB-only renders
- gain-only renders
- combined RGB+gain renders
- gain deferrals
- no-frame skips
- clean skips
- scheduler time rollbacks

Command:

    r

includes scheduler diagnostics alongside target/effective gain state.

Periodic STAT includes compact scheduler counters too.

## Native tests

RenderScheduler tests cover:

- no frame -> no render
- new RGB renders immediately
- gain-only rate limiting
- static clean frame does not rerender
- RGB+gain combines into one render
- time rollback recovery

Render profile tests also verify:

- metadata-only changes do not dirty RGB
- usable vs fail-open state does
- hidden endpoint values inside fail-open contexts do not matter

## Physical safety

Nothing in Stage 13 changes:

    ShadowRenderPolicy::physicalOutput(...)

Physical LEDs still receive original RGB.

## Next hardware gate

On real hardware:

1. run normal 60 FPS DDP
2. stop/hold RGB updates if possible
3. start shadow probe x
4. verify gain-only scheduler renders continue during slew
5. inspect r
6. confirm gain-only renders settle back to zero activity after convergence
7. confirm DDP p95/p99 is not inflated by gain-only rerenders
