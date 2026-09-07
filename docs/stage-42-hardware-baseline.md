# Stage 42: canonical hardware baseline

## Purpose

Stage 42 removes the need to cherry-pick measured TV wiring fixes into every
new development branch.

The physical panel wiring is now part of the repository's canonical hardware
configuration rather than an incidental default embedded in
`LedMappingProfile`.

Firmware identity:

- version: `0.42.1-hardware-baseline`;
- development stage: `42`.

## Sources of truth

`BoardConfig.h` owns controller lane wiring only:

- lane 0 -> GPIO18;
- lane 1 -> GPIO19;
- lane 2 -> GPIO20;
- lane 3 -> GPIO21.

`PanelConfig.h` owns installed TV panel wiring in logical perimeter order:

- TOP -> GPIO20, 230 LEDs, reversed;
- RIGHT -> GPIO19, 160 LEDs, reversed;
- BOTTOM -> GPIO21, 230 LEDs, reversed;
- LEFT -> GPIO18, 160 LEDs, forward.

`LedMappingProfile{}` derives its default PARLIO lanes from those two tables.
It no longer contains a second hand-written side/lane/direction table.

Runtime topology remains editable for commissioning. The measured panel
configuration is the canonical reset/default state, not an immutable runtime
restriction.

Runtime topology apply/reset no longer requires the operator to set brightness
to zero manually. The controller performs an explicit transaction:

1. wait for any in-flight PARLIO frame;
2. temporarily set hardware output brightness to zero without changing the
   persisted brightness setting;
3. transmit and wait for a physical black frame;
4. apply the topology across ToF, DDP, renderer and pixel-mask state;
5. persist the topology when NVS is available;
6. restore the previous hardware brightness and schedule a fresh render.

If a durable topology save fails while NVS is available, the live subsystems
are rolled back to the previous topology before brightness is restored.

## Why schema 3

Older firmware used `LedMappingProfile::kSchemaVersion == 2` while its default
mapping still assumed lane == side.

A persisted schema-2 topology can therefore override a corrected firmware
default during boot.

Stage 42 increments the schema to 3. Existing `RuntimeSettings` fail-closed
loading behavior then treats any schema-2 topology as stale:

1. the stale topology blob is not loaded;
2. its `led_map` and `led_map_ver` keys are removed;
3. the live profile remains the measured Stage 42 default;
4. any future runtime customization is persisted as schema 3.

This intentionally invalidates all schema-2 topology customizations once.
Runtime topology settings made after Stage 42 persist normally.

Pixel masks are still validated/sanitized against the active topology during
boot.

## Compile-time invariants

The hardware configuration statically verifies:

- every measured GPIO exists in the board lane table;
- every PARLIO lane is used exactly once;
- no segment exceeds the 230-pixel physical lane capacity;
- total default logical LEDs remain 780;
- default side GPIOs are exactly 20/19/21/18;
- default directions are REV/REV/REV/FWD.

## Native tests

`test_panel_config` locks the measured side/GPIO/direction contract and proves
that the default runtime mapping is derived from it.

`test_led_render_plan` no longer duplicates the hardware topology. It checks
that render-plan generation preserves the current default mapping.

`test_runtime_settings` includes two migration tests:

- a persisted schema-2 lane==side profile is rejected, deleted and replaced by
  the measured default;
- a schema-3 custom topology survives a simulated reboot.

## Build fix carried into baseline

Stage 42 also contains the explicit forward declaration for
`dumpRuntimeStatus()`, so future branches do not need to reapply that local
compile fix.

## Acceptance gate

Before using Stage 42 as the parent of subsequent stages:

1. build all native tests;
2. build the ESP32-C6 release environment;
3. flash and verify `0.42.1-hardware-baseline`;
4. confirm serial topology:
   `TOP=GPIO20:REV RIGHT=GPIO19:REV BOTTOM=GPIO21:REV LEFT=GPIO18:FWD`;
5. confirm Stage 41 PARLIO pipeline metrics remain unchanged within normal
   run-to-run variation;
6. run the accelerated ToF stability test and confirm no Guru Meditation;
7. verify a topology reset returns to the measured mapping above;
8. with brightness greater than zero, change one runtime topology value through
   Web UI and confirm a short blackout occurs automatically, brightness returns
   to its previous value, and the UI reports the mapping as saved;
9. reboot and confirm the changed schema-3 runtime topology is restored from
   NVS.

After this gate, Stage 42 should be the parent baseline for later development
branches.
