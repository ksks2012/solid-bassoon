# 0006 — Relativistic beaming

**Spec:** [../../spec/0001-relativistic-jet.md](../../spec/0001-relativistic-jet.md)

**What to build:** Bipolar relativistic beaming. With a single constant bulk velocity, the lobe moving toward the observer brightens and the receding lobe dims, via a Doppler factor computed in the static local frame from the already-carried photon momenta, raised to `(3 + α)` with a tunable spectral index. The emitter → observer direction is the reverse of the backward-traced march direction, so the sign must be verified visually.

**Blocked by:** 0005 — can start once it is done.

**Status:** Done
**Completed:** 2026-10-16 (commit `4185ca9`)

- [x] At an inclined viewing angle, the approaching lobe is visibly brighter and the receding lobe darker or absent.
- [x] The brighter lobe is the one whose velocity points toward the observer; flipping the bulk-velocity sign swaps which lobe is bright (verify at an inclined viewing angle, e.g. inclination ≈ 60–75°).
- [x] Bulk velocity and spectral index are tunable; zero velocity reproduces the unbeamed jet.
- [x] Beaming composes multiplicatively with the emission profile, with no double counting.

**See:** [archive/summaries/0006-relativistic-beaming.md](../../archive/summaries/0006-relativistic-beaming.md) · [adr/0004](../../adr/0004-jet-constant-bulk-velocity-beaming.md)
