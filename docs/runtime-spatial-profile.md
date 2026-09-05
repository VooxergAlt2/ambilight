# Runtime ToF spatial profile

## Purpose

All mounting-dependent spatial parameters can be changed without rebuilding firmware.

The profile is applied atomically to:

- VL53L5CX grid orientation
- robust plane processing
- plane-change deadband geometry
- LED perimeter projection
- per-LED wall-distance calculation

## Stored fields

The persisted profile contains:

1. LED perimeter width
2. LED perimeter height
3. ToF sensor X offset from screen centre
4. ToF sensor Y offset from screen centre
5. LED plane Z relative to ToF optical origin
6. sensor rotation quarter-turns
7. horizontal mirror flag
8. wall-plane deadband

Lengths are stored in fixed-point 0.1 mm units.

This preserves the current 1437.5 mm width without persisting compiler-dependent floating-point configuration.

## Default profile

    width       1437.5 mm
    height      1000.0 mm
    sensor X       0.0 mm
    sensor Y       0.0 mm
    LED Z          0.0 mm
    rotation          0
    mirror            0
    deadband        10.0 mm

## Serial commands

Show profile:

    y<Enter>

Set the complete profile:

    yWIDTH,HEIGHT,SENSOR_X,SENSOR_Y,LED_Z,ROT,MIRROR,DEADBAND<Enter>

Example default:

    y1437.5,1000,0,0,0,0,0,10<Enter>

Reset:

    yreset<Enter>

Length fields accept integer values or one decimal place.

Sensor X/Y/Z may be negative.

Rotation:

    0 = 0 deg
    1 = 90 deg
    2 = 180 deg
    3 = 270 deg

Mirror:

    0 = off
    1 = on

## Safety

Spatial changes are refused in ACTIVE mode.

Switch to SHADOW or DISABLED first.

When a new profile is accepted:

1. old geometry snapshot is invalidated
2. old gain snapshots become fail-open unity
3. render target/controller return to unity
4. sensor task installs the complete new profile
5. temporal band filters reset if orientation changed
6. PlaneChangeGate reference resets
7. next valid pose forces a new plane/perimeter rebuild

There is no mixed interval where the plane uses one coordinate system while the LED perimeter uses another.

## NVS

Namespace:

    ambilight

Keys:

    spatial
    spatial_ver

Schema version:

    1

The profile blob is written first.

Version is written last and acts as the commit marker.

At boot the stored profile is accepted only when:

- schema version matches
- blob size matches exactly
- all value bounds validate

Otherwise both keys are removed and firmware falls back to the default profile.

## Validation bounds

Current defensive bounds:

- width/height: 100..5000 mm
- sensor X/Y: -2000..+2000 mm
- LED Z: -1000..+1000 mm
- deadband: 1..500 mm
- rotation: 0..3
- mirror: 0..1

These limits are corruption guards, not recommended installation values.

## Runtime source

Diagnostics distinguish:

    DEFAULT
    CUSTOM_NVS
    CUSTOM_RUNTIME

CUSTOM_RUNTIME means the profile is active for the current boot but NVS persistence was unavailable or failed.

## Hardware workflow later

Physical commissioning can now change mounting parameters without compiling firmware:

1. verify raw sensor orientation
2. adjust rotation/mirror
3. measure LED perimeter
4. measure sensor X/Y/Z offsets
5. tune plane deadband from real noise
6. save profile
7. proceed to photometric curve calibration
