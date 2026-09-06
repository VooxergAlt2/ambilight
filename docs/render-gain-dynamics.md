# Gain dynamics

## Purpose

The wall plane updates slowly, but a visible brightness transition should remain smooth after an accepted geometry change.

The controller therefore separates:

    sparse target updates
        from
    smooth effective gain updates

## Current timing

Normal ToF plane candidate:

    about every 12 seconds

Plane changes below the screen-space deadband:

    no gain rebuild
    no transition

Material plane change:

    new active per-LED target
    smooth slew toward target

Effective gain/render update while moving:

    up to ~60 Hz

## Slew model

Current setting:

    8192 Q12 units per second

Unity is 4096.

A full-scale transition would take roughly 0.5 seconds.

All active logical gains slew independently.

## Fail-open behavior

Valid target changes:

    smooth toward target

Invalid/stale/missing spatial source:

    snap immediately to unity

Fresh reacquisition:

    slew from unity toward the new accepted target

## Why this still matters with slow sensing

The sensor does not need to run at video rate.

Once a new TV pose is detected, the output transition itself can still be visually smooth and deterministic.

## Shadow/activation boundary

The current branch still keeps physical output protected by ShadowRenderPolicy while the software stack is completed.

Hardware measurements are deferred validation and calibration work, not blockers for implementing the remaining production path.
