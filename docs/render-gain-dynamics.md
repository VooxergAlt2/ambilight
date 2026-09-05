# Stage 12: shadow gain dynamics

## Purpose

Stage 12 models the time-domain behavior of future ToF brightness correction while physical RGB remains completely unchanged.

Stage 11 proved:

    GainSnapshot -> RenderGainContext -> per-pixel shadow math

Stage 12 adds:

    target RenderGainContext
      -> RenderGainController
      -> effective RenderGainContext
      -> LedRenderer shadow math

## Why a render-side controller is needed

VL53L5CX geometry/gains update around 10 Hz.

LED frames render around 60 Hz.

If future physical correction simply used the latest 10 Hz gain value directly, brightness would change in visible 100 ms steps.

The controller converts those discrete target updates into per-frame effective gains.

## Slew model

Current shadow setting:

    8192 Q12 units per second

Since unity is 4096, a full 0% -> 100% range would take about 500 ms.

A 100% -> 50% transition needs about 250 ms.

Each segment start/end endpoint slews independently.

This is deliberately time-based, not frame-count-based, so behavior remains stable if render FPS changes.

## Fail-open behavior

Fail-open is intentionally asymmetric.

Valid target changes:

    smooth toward target

Invalid/stale/missing target:

    snap immediately to unity

Reason:

- old attenuation must never be held after ToF data becomes unsafe
- sensor failure must not leave one side dim
- safety takes priority over visual smoothness on failure

When ToF becomes valid again:

1. first valid render remains at unity
2. subsequent frames slew toward the target

Time while the sensor was invalid is not retroactively counted toward attenuation.

## Time rollback

If the monotonic timestamp unexpectedly moves backward, the controller:

- records a rollback
- resets effective gains to unity
- restarts valid slew from there

This keeps unexpected clock behavior fail-safe.

## Target versus effective context

The controller exposes both:

- target context, from ToF/probe
- effective context, actually evaluated by LedRenderer shadow math

Debug command:

    r

prints both.

This allows direct observation of:

- 10 Hz target steps
- 60 Hz effective interpolation
- fail-open snaps
- endpoint gradients

## Shadow probe

Debug command:

    x

starts a deterministic 10-second aggressive shadow profile:

    TOP:    100% -> 75%
    RIGHT:   75%
    BOTTOM:  50% -> 100%
    LEFT:    25%

The probe exists only to exercise:

- per-segment mapping
- gradients
- slew
- changed-pixel counters
- CPU timing
- target/effective diagnostics

The profile does not alter ToF calibration and does not alter physical RGB.

When the probe ends, the controller returns toward the real ToF target.

If the real target is fail-open, return to unity is immediate.

## Physical safety

Stage 12 still ends with:

    ShadowRenderPolicy::physicalOutput(original, candidate)
        -> original

There is no runtime activation switch.

The new shadow probe therefore cannot change visible LEDs.

## Metrics

Renderer:

- would-change pixels
- changed pixels per segment
- max channel delta
- shadow/original RGB ratio
- prepare time

Controller:

- update count
- usable target count
- fail-open target count
- target generation changes
- fail-open unity snaps
- time rollbacks
- maximum endpoint step

Runtime status also reports whether shadow probe is active.

## Native tests

RenderGainController tests cover:

- first valid target starts at unity
- real-time slew
- exact target convergence
- fail-open immediate unity
- valid reacquisition
- time rollback
- independent gradient endpoint slew
- deterministic probe profile

## Activation gate

Even after Stage 12, physical correction remains blocked.

Before changing ShadowRenderPolicy:

1. run x and inspect r on real 60 FPS HyperHDR content
2. confirm prepare-time overhead is acceptable
3. collect real c calibration captures
4. install a real ToF gain curve while remaining in shadow mode
5. compare target vs effective profiles
6. verify screen-space direction of each logical segment
7. verify DDP p95/p99 does not regress materially
