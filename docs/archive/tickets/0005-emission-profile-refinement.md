# 0005 — Jet emission profile refinement

**Spec:** [../../spec/0001-relativistic-jet.md](../../spec/0001-relativistic-jet.md)

**What to build:** A realistic brightness distribution for the jet: a bounded axial power-law falloff (finite at the base, declining outward), a soft fade near the top so there is no hard cut-off, tunable radial thickness via sigma, a reserved shape exponent, and a cheap bounding test that skips the jet kernel for rays far from the jet. Reserving the shape exponent lets the cone be turned into a parabola later by changing one parameter.

**Blocked by:** 0004 — can start once it is done.

**Status:** Done
**Completed:** 2026-10-15 (commit `37ff62a`)

- [x] The jet is brightest near its base and fades smoothly with height, with no hard cut-off at the top.
- [x] Brightness stays finite at the base (no blow-up or hot spot on the axis at z = 0).
- [x] Radial thickness and the shape exponent are tunable.
- [x] Rays far from the jet skip the jet kernel (measurable performance benefit).
- [x] The cone can be made parabolic by changing only the exponent parameter.

**See:** [archive/summaries/0005-emission-profile-refinement.md](../../archive/summaries/0005-emission-profile-refinement.md)
