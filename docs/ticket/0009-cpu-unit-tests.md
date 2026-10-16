# 0009 — CPU unit tests for metric, integrator, and config

**Spec:** [../spec/0002-testing-and-performance-harness.md](../spec/0002-testing-and-performance-harness.md)

**What to build:** The base of the test pyramid: exact, fast unit tests for the pure core, run under CTest with **no GL context**.

**Blocked by:** 0008 — can start once the core library is linkable.

**Status:** Todo

- [ ] **Metric**: inverse identity `g_μν·g^νλ = δ`; constants (`rs`, photon sphere, ISCO); `geodesicRHS` conserves `p_t`/`p_φ`; `computeNullPr` yields a null vector; horizon-boundary behaviour.
- [ ] **Integrator**: null-condition drift stays small; `p_t`/`p_φ` conserved; capture/escape/`MAX_STEPS` classification; weak-field deflection `≈ 4M/b`; RK4 error scales `≈ h⁴`; `adaptiveStep` monotone and floored/capped.
- [ ] **Config**: struct defaults; overrides applied; unknown keys ignored; missing file and malformed JSON raise `ConfigError`.
- [ ] All tests are deterministic, require no GL context, and run in well under a second.

**See:** [../standards/testing-and-performance.md](../standards/testing-and-performance.md) · [../adr/0005-extract-blackhole-core.md](../adr/0005-extract-blackhole-core.md)
