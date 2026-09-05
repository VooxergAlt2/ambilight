# ESP32-C6 Ambilight Controller

Custom ESP32-C6 Ambilight endpoint for HyperHDR.

## Current stage

Stage 11 runs one PC over Wi-Fi/DDP and feeds ToF gains into the renderer in **shadow mode only**.

Physical LEDs still receive the original HyperHDR RGB.

    HyperHDR
      -> Wi-Fi/DDP
      -> RgbFrame
      -> LedRenderer
      -> ORIGINAL RGB
      -> PARLIO x4

In parallel:

    VL53L5CX
      -> geometry
      -> gain model
      -> RenderGainContext
      -> shadow RGB calculation only

## Render gain mechanics

Gain format:

    4096 = 100%

Each logical segment has start/end gain endpoints.

Current gain model supplies uniform endpoints, but the renderer already supports a logical gradient along a segment for future yaw/plane compensation.

Gain math:

    channel_out = round(channel_in * gain / 4096)

R, G and B use the same gain.

## Hard shadow safety

There is no runtime switch that enables correction.

`ShadowRenderPolicy` always returns the original RGB for physical output.

Native tests assert this contract.

## Frame synchronization

For every new RGB frame the controller copies one small GainSnapshot, releases the ToF mutex, creates an immutable RenderGainContext and uses it for all 780 LEDs.

ToF cannot change coefficients halfway through one frame.

## Debug commands

    t

Raw 8x8 ToF map.

    g

Processed LEFT/CENTER/RIGHT geometry.

    k

ToF gain snapshot.

    c

Five-second calibration capture.

    r

Renderer shadow diagnostics, including:

- source age/usability
- segment start/end gains
- pixels that would change
- maximum channel delta
- shadow/original RGB ratio
- renderer preparation time

## Current calibration

Still pass-through:

    50 mm   -> 100%
    4000 mm -> 100%

So expected Stage 11 shadow result is 0 changed pixels while all future math is exercised.

## USB/AWA

Preserved separately in:

    stage/07-usb-awa

Active development remains Wi-Fi/DDP.

## Next gate

Collect real ToF calibration data, install a real attenuation curve while staying in shadow mode, verify segment orientation and render timing, then consider a separate physical-application stage.
