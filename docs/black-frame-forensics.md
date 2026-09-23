# Black-frame forensics

Stage 45.2 adds observational black-frame diagnostics without changing render decisions, DDP acceptance, ToF correction or PARLIO output.

The tracker runs only after `LedRenderer` successfully submits a frame through `LedEngine::show()`.

For each rendered frame it records:

- source non-zero pixel count and maximum RGB channel;
- age of the source DDP frame at the moment it is rendered;
- post-gain non-zero pixel count;
- prepared-output non-zero pixel count and maximum RGB channel;
- active gain minimum/maximum Q12;
- number of logical LEDs whose gain is exactly zero;
- source DDP generation;
- timestamp and correction-active state.

A complete black-output transition is classified as one of:

- `SOURCE_BLACK`: the DDP RGB frame itself is completely black;
- `ACTIVE_GAIN`: source RGB is non-zero but ACTIVE gain reduces every output pixel to zero;
- `BRIGHTNESS_ZERO`: global LED brightness is explicitly zero;
- `PIXEL_MASK`: non-zero post-gain data exists but the configured disabled-pixel mask removes all remaining lit pixels;
- `UNKNOWN`: defensive fallback for an unclassified complete-black result.

`scale8_video()` in LiteLED guarantees that a non-zero prepared RGB channel remains non-zero for any non-zero effective whole-output brightness. Therefore exact complete-black classification before the PARLIO brightness scaler is valid as long as brightness itself is handled separately, which the tracker does.

On the transition into black, Serial prints one line similar to:

    BLACK EVENT reason=SOURCE_BLACK gen=1234 src_age_ms=4 pixels=780 src_nonzero=0 src_max=0 after_gain_nonzero=0 out_nonzero=0 out_max=0 gain=4096..4096 gain_zero=0 brightness=253 active=yes events=1

A small `src_age_ms` on `SOURCE_BLACK` means the black RGB was freshly received from HyperHDR. A large source age combined with `ACTIVE_GAIN` means an older held RGB frame was re-rendered because correction state changed.

On recovery it prints:

    BLACK RECOVERY gen=1235 black_frames=1 last_black_age_ms=10 src_age_ms=3 src_max=190 out_max=190

The in-memory `BlackFrameForensicsStats` remains accessible through:

    renderer.blackFrameForensics().stats()

The latest black sample is retained until another black sample replaces it or the controller reboots.

The tracker is diagnostic only. It does not reject legitimate black movie frames and does not hold, alter or synthesize RGB data.
