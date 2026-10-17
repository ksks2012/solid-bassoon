# 0009 — CPU unit tests for metric, integrator, and config

**Spec:** [../spec/0002-testing-and-performance-harness.md](../spec/0002-testing-and-performance-harness.md)

**What to build:** The base of the test pyramid: exact, fast unit tests for the pure core, run under CTest with **no GL context**.

**Blocked by:** 0008 — can start once the core library is linkable.

**Status:** Done · Completed: 2026-10-17

- [x] **Metric** (`tests/unit/metric_test.cpp`): inverse identity `g_μν·g^νλ = δ` across radii/angles; constants (`rs`, photon sphere, ISCO) for M=1 and M=2; `geodesicRHS` gives `dp[0]=dp[3]=0` and matches analytic components; `computeNullPr` yields a null vector (both signs) plus the horizon-return clamp for over-large `b`; horizon/escape boundaries.
- [x] **Integrator** (`tests/unit/integrator_test.cpp`): `p_t`/`p_φ` conserved exactly and null-condition drift `≲1e-12`; CAPTURED / ESCAPED / MAX_STEPS plus the two immediate zero-step classifications; weak-field deflection `4M/b` within ~6.4%; RK4 error ratio ≈16 (4th order) across four step sizes; `adaptiveStep` capped, floored, known linear value, and monotone.
- [x] **Config** (`config_test.cpp`): struct defaults; every documented key applied; unknown keys/sections ignored with defaults preserved; missing file and malformed JSON raise `ConfigError`.
- [x] All tests are deterministic across repeated runs, require no GL context (no GL/GLFW/GLEW linked), and the whole suite runs in ~0.02 s.

**See:** [../standards/testing-and-performance.md](../standards/testing-and-performance.md) · [../adr/0005-extract-blackhole-core.md](../adr/0005-extract-blackhole-core.md)
