# Stage 16 note: extrapolation confidence retired

Stage 16 introduced an extrapolation ratio comparing the ToF-observed wall footprint with the configured LED perimeter.

Stage 19 removes that ratio from the spatial gain model.

## Current rule

The wall plane is the spatial model.

Once the robust plane is valid and fresh, every LED point is projected along +Z onto that plane.

The directly observed wall footprint:

- may remain useful as a diagnostic of the plane estimator
- does not scale gain
- does not modify projected distance
- does not trigger a projection warning
- does not fail open by itself

## Projection validity

Projection is usable when:

1. the wall plane is valid and fresh
2. the configured logical screen geometry is valid
3. every LED +Z ray produces a finite wall distance
4. every distance lies inside the configured physical distance range

This keeps the geometry model simple: fit the wall plane first, then derive the wall position behind the whole LED perimeter from that plane.
