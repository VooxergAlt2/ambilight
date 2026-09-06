# Runtime ToF gain calibration

## Purpose

The distance-to-brightness curve can be changed without rebuilding firmware.

This separates firmware completion from later physical photometric calibration.

## Curve model

Each point is:

    distance_mm : gain_Q12

Unity:

    4096 = 100%

Example:

    2048 = 50%

Rules:

- 2..8 points
- distance strictly increasing
- gain <= 4096
- gain non-decreasing as distance increases

The model only attenuates. It never boosts above the HyperHDR input.

## Serial commands

Show current curve:

    q<Enter>

Set a curve:

    q50:2048,500:3072,4000:4096<Enter>

Restore firmware default:

    qreset<Enter>

No spaces are accepted inside the curve command.

## Safety boundary

Curve changes are refused in:

    ACTIVE

Switch to:

    SHADOW

or:

    DISABLED

before editing calibration.

This prevents a calibration edit from changing physical LED output unexpectedly.

## Live update sequence

When a valid new curve is accepted:

1. TofService queues the new curve under mutex
2. current gain snapshots immediately become fail-open unity
3. render target/controller return to unity
4. ToF task installs the new curve
5. plane-change reference is reset
6. next valid ToF pose forces a full active-LED rebuild
7. normal deadband resumes afterward

An old curve therefore cannot remain physically active while a new calibration waits for the next pose.

## NVS storage

Namespace:

    ambilight

Keys:

    tof_curve
    tof_count

The complete fixed-size point array is written first.

The point count is written last and acts as the commit marker.

At startup the stored data is accepted only if:

- count is 2..8
- byte size is exact
- DistanceGainCurve validation succeeds

Invalid stored data is removed and firmware falls back to the compiled default curve.

## Profile source diagnostics

The runtime reports:

    DEFAULT
    CUSTOM_NVS
    CUSTOM_RUNTIME

CUSTOM_RUNTIME means the curve is active for the current boot but persistence failed or NVS is unavailable.

## Startup

RuntimeSettings loads the selected curve before TofService task startup.

The ToF service therefore begins with the correct curve and does not need a post-boot transition from default to custom.

## Default profile

Until real photometric measurements are available:

    50:4096
    4000:4096

This is intentionally neutral.

The runtime calibration mechanism is complete even though the physical coefficients are not known yet.
