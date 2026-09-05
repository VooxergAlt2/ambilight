# Runtime LED mapping profile

## Purpose

Physical strip wiring can differ from the logical HyperHDR perimeter.

Stage 27 makes two mounting-dependent properties configurable without rebuilding firmware:

- logical segment -> PARLIO lane
- logical segment direction (forward/reversed)

GPIO numbers and segment lengths remain compile-time constants.

## Logical segments

The logical RGB frame remains fixed:

    TOP     0..229
    RIGHT   230..389
    BOTTOM  390..619
    LEFT    620..779

Changing the runtime map does not change HyperHDR geometry or ToF logical coordinates.

## Default map

    TOP     lane 0 FWD
    RIGHT   lane 1 FWD
    BOTTOM  lane 2 FWD
    LEFT    lane 3 FWD

## Serial commands

Show active map:

    l<Enter>

Set map:

    lTlane:Trev,Rlane:Rrev,Blane:Brev,Llane:Lrev<Enter>

Example default:

    l0:0,1:0,2:0,3:0<Enter>

Example with TOP on lane 3 reversed:

    l3:1,1:0,2:0,0:0<Enter>

Reset:

    lreset<Enter>

Reversal flag:

    0 = forward
    1 = reversed

## Validation

A valid map requires:

- lane in 0..3
- reversal in 0..1
- every physical lane used exactly once

Duplicate lanes are rejected.

## Safety boundary

Mapping edits are accepted only when:

    output brightness = 0

This prevents a lane permutation from becoming visible unexpectedly while live content is displayed.

Recommended commissioning flow:

1. b0
2. change mapping
3. verify status
4. restore a low brightness
5. run commissioning pattern / HyperHDR test

## Runtime behavior

LedRenderer owns the active mapping profile.

For every logical LED:

1. determine logical segment/offset
2. apply runtime lane selection
3. apply runtime reversal
4. send the resulting physical lane/index to LedEngine

ToF gains remain indexed in logical screen order before this physical mapping layer.

Therefore a reversed strip cannot reverse the wall-distance correction model.

## NVS

Namespace:

    ambilight

Keys:

    led_map
    led_map_ver

Schema:

    1

The mapping blob is written first.

Version is written last as the commit marker.

Invalid stored profiles are removed and firmware falls back to the default map.

Diagnostics distinguish:

    DEFAULT
    CUSTOM_NVS
    CUSTOM_RUNTIME
