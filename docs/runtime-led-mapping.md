# Runtime LED topology profile

## Purpose

The controller keeps logical TV geometry separate from physical wiring. Each of
the four logical sides stores:

- LED count;
- physical PARLIO output/GPIO;
- FWD/REV strip direction.

No firmware rebuild is required when those values change.

## Capacity model

There is **no separate aggregate LED-count ceiling** such as the old 920-address
limit. The sum is derived from four `uint16_t` side fields, so the persisted
format itself can represent up to 262,140 logical LEDs total.

Each individual side length is persisted as `uint16_t`, so the configuration
format represents `1..65535` LEDs per side. Aggregate logical indices and frame
sizes use wider runtime types and do not wrap at 65535 total LEDs.

The practical controller limit is resource-based:

- free ESP32-C6 heap for RGB, DDP and optional gain fields;
- PARLIO/LiteLED lane buffer allocation;
- physical LED wire time and the resulting maximum frame rate.

Topology changes pre-allocate the required main RGB buffers before the physical
mapping is changed. DDP staging/coverage and PARLIO lane storage are then resized
transactionally. If required memory cannot be obtained, the new topology is
rejected rather than partially applied.

**Hardware testing has been performed up to 230 LEDs on a single output.**
Values above 230 are accepted by the software path but remain experimental on
real LED hardware.

The measured/default installation remains:

    TOP     230 : GPIO20 : REV
    RIGHT   160 : GPIO19 : REV
    BOTTOM  230 : GPIO21 : REV
    LEFT    160 : GPIO18 : FWD

Logical order is always:

    TOP -> RIGHT -> BOTTOM -> LEFT

Therefore:

    active DDP bytes = totalLedCount * 3

There are no logical holes.

## Serial commands

Status:

    l<Enter>

Set:

    lCOUNT:GPIO:REV,COUNT:GPIO:REV,COUNT:GPIO:REV,COUNT:GPIO:REV<Enter>

Measured-default example:

    l230:20:1,160:19:1,230:21:1,160:18:0<Enter>

Reset:

    lreset<Enter>

Validation rules:

- `COUNT`: `1..65535` for each side (storage-format range, not a promise that an
  extreme value will fit ESP32-C6 memory);
- `GPIO`: 18, 19, 20 or 21;
- every GPIO must be assigned exactly once;
- `REV`: 0 or 1.

## Coordinated topology transaction

A normal topology change is controller-owned and performs a safety blackout.
The operator does not need to set brightness to zero first.

The transaction is:

1. pre-allocate RGB frames for the requested aggregate topology;
2. cancel active commissioning and enter a physical blackout;
3. sanitize the physical service-LED hole for the requested topology, then resize PARLIO lanes to the longest resulting **physical** side (`logicalLength + 1` on a side with a hole);
4. queue the topology to ToF;
5. resize DDP staging/coverage to `totalLedCount * 3` and reset the sender epoch;
6. switch renderer logical-to-physical mapping;
7. commit the sanitized physical-hole profile for any side whose new topology can no longer represent its stored offset;
8. persist/reset topology in NVS;
9. install the pre-allocated runtime RGB frames and reset RGB generation state;
10. reset dynamic gain storage; if optional ToF gain storage cannot be allocated,
    correction fails open while normal DDP rendering remains available;
11. restore configured output brightness.

Every intermediate failure requests rollback to the previously active topology.
If a low-level physical rollback itself fails, output is deliberately kept black
and a reboot is required rather than illuminating a mixed topology.

## Physical GPIO model

The four fixed PARLIO outputs are:

    lane 0 -> GPIO18
    lane 1 -> GPIO19
    lane 2 -> GPIO20
    lane 3 -> GPIO21

All four PARLIO lane buffers use the length of the **longest active side**.
This is important: allowing a long strip does not make the normal 230/160
reference topology transmit 65535 empty pixel slots every frame.

Raw GPIO commissioning tests are bounded by the currently allocated physical
lane length.

## ToF independence from wiring

ToF distance/gain is calculated in logical screen order. Physical GPIO mapping
and REV are applied afterward, so rewiring/reversing a strip cannot reverse the
wall model.

Per-pixel gain buffers are dynamic and follow the active logical topology. If
the optional gain field cannot allocate memory, correction is fail-open rather
than blocking normal Ambilight output.

## NVS compatibility

Namespace:

    ambilight

Keys:

    led_map
    led_map_ver

Schema remains:

    3

Removing the old 230/920 policy limits did **not** change the persisted
`LedMappingProfile` blob: it still stores four `uint16` side lengths plus lane
and direction fields. Existing v0.46.3 settings therefore need no migration and
are not reset by v0.46.4.
