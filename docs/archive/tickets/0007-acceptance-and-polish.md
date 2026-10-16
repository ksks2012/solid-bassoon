# 0007 — Acceptance captures, presets, and polish

**Spec:** [../../spec/0001-relativistic-jet.md](../../spec/0001-relativistic-jet.md)

**What to build:** The P2.1 acceptance artifact: a fixed-camera reference set (inclined beaming, edge-on geometry, near-axis fade, jet-off regression), a documented tuned preset, and parameter/README polish. Records whether the feature passes its acceptance criteria.

**Blocked by:** 0006 — can start once it is done.

**Status:** Done
**Completed:** 2026-10-16 (commit `177722f`)

- [x] Fixed-camera golden reference images exist for the four acceptance shots (inclined, edge-on, near-axis, jet-off).
- [x] The jet-off regression differs from the pre-change render only by the disk inner edge (ISCO).
- [x] Frame-time delta (jet on vs off) at the target resolution is ≤ 20%.
- [x] A tuned preset is documented for opt-in use.
- [x] All P2.1 acceptance criteria are recorded as pass/fail in the spec.

**See:** [archive/summaries/0007-acceptance-and-polish.md](../../archive/summaries/0007-acceptance-and-polish.md)
