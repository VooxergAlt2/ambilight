# Render dirty scheduler

## Purpose

Rendering may be triggered by video state or environmental gain state.

The latest RGB frame is cached so an accepted ToF geometry change can update brightness even when the image itself is static.

## Render causes

A render may be requested by:

    new RGB frame

or:

    changed/unsettled gain profile

or both.

## Gain target polling

The main loop polls the cached ToF gain field at:

    1 Hz

This is intentionally much faster than the normal ~12-second pose update while avoiding needless repeated copies of a 780-value field.

Metadata-only changes do not dirty the render profile.

Therefore a fresh ToF measurement that falls inside the plane deadband can refresh generation/timestamp without causing a state-only rerender.

## State-only render rate

Accepted gain changes slew at up to approximately:

    60 Hz

Fresh RGB frames bypass the state-only limiter and render immediately.

## Network latency isolation

State-only rerenders reuse cached RGB and do not enter DDP frame-age statistics.

## Fail-open

Usable unity and fail-open unity are different states.

A fail-open transition can therefore force the renderer/controller back to unity even though the numerical target happens to be 4096.

## Native tests

Scheduler tests cover:

- no frame
- new RGB
- state-only rate limiting
- RGB + gain combined
- clean static frame
- time rollback

Render-profile tests verify that metadata-only source refresh does not cause needless rendering.
