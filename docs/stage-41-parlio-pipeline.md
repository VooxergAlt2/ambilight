# Stage 41: PARLIO double-buffer pipeline

## Goal

Overlap CPU preparation and encoding of LED frame N+1 with the physical WS2812
transfer of frame N without introducing a stale-frame FIFO.

The physical wire interval for the current 230-pixel longest lane remains about
7.02 ms. Stage 41 does not change that limit. It reduces how much of that time
blocks the single ESP32-C6 application core.

## Hardware baseline carried forward

The Stage 41 branch already includes the hardware-proven non-topology changes:

- commissioning timeout: 120 seconds;
- commissioning brightness ceiling: 255;
- explicit forward declaration for `dumpRuntimeStatus()`;
- ToF task stack: 20 KiB.

The measured topology commits supplied from the local Stage 39/40 work are not
present in the remote GitHub history:

- `29f2a00 fix(led): align default topology with measured panel wiring`;
- `0a877bd fix(test): align render plan default with measured topology`.

Before flashing a release candidate, those two local changes must be carried
onto this branch (or pushed so they can be merged). Stage 41 intentionally does
not touch `LedMappingProfile` or its render-plan tests, so that integration
should be conflict-free.

Known measured side-to-GPIO mapping:

- TOP -> GPIO20;
- RIGHT -> GPIO19;
- BOTTOM -> GPIO21;
- LEFT -> GPIO18.

Three of the four sides are reversed relative to the logical clockwise
TOP -> RIGHT -> BOTTOM -> LEFT perimeter convention. The exact three REV flags
must come from the hardware-proven local commit, not from a guess.

## Driver model

Two DMA-capable internal-RAM bitstream buffers are allocated.

At steady state:

1. PARLIO owns DMA buffer A and transmits frame N.
2. CPU renders RGB N+1 into persistent lane pixel buffers.
3. CPU encodes N+1 into free DMA buffer B.
4. CPU waits only for any remaining time of frame N.
5. Buffer B is submitted.
6. Function returns immediately and the next loop iteration starts while B is
   on the wire.

The buffers alternate A/B.

The hardware PARLIO transmit queue depth is intentionally 1. There is never
more than one hardware-owned frame and no queue of N+2/N+3 stale frames.

## Ownership invariants

The host-testable `ll_parlio_pipeline.h` state machine enforces:

- an in-flight DMA buffer can never be selected for encoding;
- only one encoded-ready buffer exists;
- transmit is impossible while an older buffer is in flight;
- waiting releases the hardware-owned buffer;
- buffers alternate without aliasing;
- reset returns to cold-start buffer 0.

The existing bit-plane encoder still overwrites every dynamic sample position
on every encode, while constant samples and reset-low regions remain initialized
once per DMA buffer.

## Memory cost

Current byte-wide PARLIO encoding uses approximately 17,560 bytes per DMA
buffer for a 230-pixel RGB lane group.

Stage 41 therefore uses approximately:

- 2 x 17,560 bytes = 35,120 bytes of internal DMA-capable RAM for bitstreams;
- plus the existing per-lane RGB buffers and normal ESP32 runtime allocations.

The exact runtime values are printed by the `PIPE` status line as
`dma=<count>x<bytes>B total=<bytes>B`.

## Runtime telemetry

Full runtime status (`u`) prints:

- `submitted`;
- `completed`;
- `overlapped`;
- `cold`;
- overlap percentage;
- DMA buffer count and bytes;
- `encode_p95`;
- `wait_residual_p95`;
- `submit_p95`;
- pipeline service p95;
- explicit flush-wait p95.

The performance dump also reports:

- `parlio_encode`;
- `parlio_wait_residual`;
- `parlio_wait_flush`;
- `parlio_submit`;
- `parlio_pipeline_service`.

At steady DDP load, `completed` may lag `submitted` by one because the latest
frame can still be physically in flight when status is sampled.

## Hardware acceptance gate

Before enabling any further PARLIO packing optimization:

1. Verify firmware identity reports `0.41.0-parlio-pipeline`, Stage 41.
2. Run normal DDP at 60, 90 and 120 fps input.
3. Run an overload input around 200 fps and confirm output no longer collapses
   through backlog starvation.
4. Capture `r` and `u` after each load point.
5. Confirm no Guru Meditation, watchdog reset, Wi-Fi loss or LED corruption.
6. Confirm `overlapped / submitted` approaches steady-state near 100% after the
   cold start.
7. Confirm `wait_residual_p95` is materially below the old blocking wire wait
   when renderer + encoder consume useful overlap time.
8. Record free heap and minimum free heap; the extra DMA buffer must not create
   memory pressure.
9. Run full-white commissioning at brightness 255 long enough to exercise the
   hardware-proven 120-second commissioning path.
10. Exercise SHADOW and ACTIVE ToF modes to confirm the 20 KiB ToF task remains
    stable alongside the larger DMA footprint.

## Rollback condition

Revert Stage 41 and return to blocking Stage 40 if any of the following occurs:

- DMA allocation failure;
- Guru Meditation or watchdog reset;
- unstable Wi-Fi caused by internal-RAM pressure;
- corrupted or reordered LED frames;
- visible black/intermediate frames during ordinary DDP operation;
- pipeline service p95 is not better than the blocking Stage 40 result.

## Deferred optimization

`data_width=4` packing remains deferred. It changes PARLIO sample packing and
requires logic-analyzer verification before it should be used on the physical
strip.
