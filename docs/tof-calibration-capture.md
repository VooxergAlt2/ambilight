# Slow pose calibration capture

## Purpose

Capture several independent stable TV-pose measurements using the normal slow ToF cadence.

Physical RGB remains unchanged.

## Runtime command

Send:

    c

The controller records unique ToF geometry frames for 60 seconds.

Normal wall-pose processing is about once every 12 seconds, so a capture should contain several independent measurements without continuous sensor processing.

## Stored plane data

For each valid plane:

- yaw
- pitch
- wall-plane intercept
- residual MAD
- accepted-zone count
- observed wall half-span as estimator diagnostics

## Stored wall-distance data

For every usable spatial snapshot:

- minimum LED-wall distance
- maximum LED-wall distance
- start/end distance of TOP
- start/end distance of RIGHT
- start/end distance of BOTTOM
- start/end distance of LEFT

These distances are derived from LED +Z intersections with the fitted plane.

No extrapolation ratio or warning is stored.

## Summary

The command prints p10 / median / p90 for:

- yaw
- pitch
- intercept
- min/max perimeter distance
- start/end distance of each segment

It also prints residual quality and accepted-zone range.

## Safety

Calibration capture is observational only.

It cannot enable physical gain application.
