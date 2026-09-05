# Firmware identity

## Purpose

Commissioning needs a deterministic way to identify the firmware running on the ESP32-C6.

Before Stage 34, the startup stage text was handwritten in main.cpp and could become stale.

Stage 34 centralizes firmware identity in:

    include/config/FirmwareInfo.h

## Current identity

    name            ambilight-c6
    version         0.34.0-dev
    development     Stage 34
    target          ESP32-C6
    serial protocol 1

## Serial command

Immediate command:

    v

or:

    V

prints firmware identity and the important runtime schema versions.

Example fields:

    name
    version
    stage
    target
    serial_proto
    logical_leds
    ddp_port
    spatial_schema
    ledmap_schema

No Enter is required.

## Startup

The startup banner is generated from FirmwareInfo constants.

main.cpp no longer contains a handwritten Stage string.

## Periodic diagnostics

STATCFG includes:

    fw
    stage

This makes captured logs self-identifying.

## Persistent schema visibility

The firmware status command also reports:

    TofSpatialProfile::kSchemaVersion
    LedMappingProfile::kSchemaVersion

These are the two versioned persisted binary profile formats.

Other simple Preferences values remain key/value settings rather than versioned binary schemas.

## Build identity scope

Stage 34 intentionally does not inject Git SHA, build time or dirty-tree state.

Reasons:

- avoid adding a fragile SCons/git subprocess dependency before first local validation
- avoid non-reproducible __DATE__/__TIME__ metadata
- keep firmware identity deterministic from source

An exact Git SHA can be added later after the local build harness has been exercised on the real development workstation.
