# 0011 — Golden regression harness + first references

**Spec:** [../spec/0002-testing-and-performance-harness.md](../spec/0002-testing-and-performance-harness.md)

**What to build:** The regression layer: a committed comparison tool and a fixed reference set over the canned scenes, plus the two cheapest, highest-value invariants. This milestone gets golden regression actually running.

**Blocked by:** 0010 — needs the headless render + readback seam.

**Status:** Todo

- [ ] A committed comparator (`tests/gpu/compare.py`, stdlib only) reports maximum absolute difference, mean absolute difference, and the fraction of elements above a threshold, operating on the raw `float32` buffers.
- [ ] The P2.1 acceptance shots are migrated from `fig/ticket-0007-*` into `tests/reference/` as raw buffers + viewable PPMs, with shape/channel sidecars.
- [ ] **jet-off byte-identity**: the jet-off render matches the pre-jet baseline exactly.
- [ ] **disk on/off localized delta**: differences are confined to the disk footprint.
- [ ] The **determinism** check (render twice ⇒ identical buffer) is wired.
- [ ] The golden-update policy is enforced explicitly: a reference change ships in the **same commit** as the code change and records the reason; a reference that changes without a code change is a defect, not a refresh.

**See:** [../standards/testing-and-performance.md](../standards/testing-and-performance.md) · [../adr/0005-extract-blackhole-core.md](../adr/0005-extract-blackhole-core.md)
