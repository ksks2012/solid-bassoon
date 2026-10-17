# Execution Summary: 0009-cpu-unit-tests

- **Date**: 2026-10-17
- **Spec/ADR Path**: `docs/spec/0002-testing-and-performance-harness.md`, `docs/adr/0005-extract-blackhole-core.md`
- **Ticket Path**: `docs/ticket/0009-cpu-unit-tests.md`
- **Branch**: `main`

---

## 📌 Overview
- **Commits**: 0 (uncommitted working-tree changes vs `HEAD` `5083202`)
- **Files Changed**: 8 files (`+464` / `-14`)

## 📝 Key Changes
- Added three committed CTest executables linking `blackhole_core` only:
  `tests/unit/metric_test.cpp`, `tests/unit/integrator_test.cpp`,
  `tests/unit/config_test.cpp`.
- **Metric**: inverse identity `g_μν g^νλ = δ` over radii/angles; constants for
  M=1 and M=2; `geodesicRHS` conserves `p_t`/`p_φ` and matches analytic
  `dx[1]`, `dx[3]`, `dp[1]`; `computeNullPr` produces a null vector (both
  signs) plus the documented over-large-`b` clamp; horizon/escape boundaries.
- **Integrator**: `p_t`/`p_φ` conserved exactly, null drift `≤1e-12`;
  CAPTURED/ESCAPED/MAX_STEPS plus two zero-step classifications; weak-field
  deflection `4M/b` (measured +6.46%); RK4 4th-order ratio ≈16; `adaptiveStep`
  capped/floored/monotone.
- **Config**: struct defaults, every documented key applied, unknown keys
  ignored, missing file and malformed JSON raise `ConfigError`.
- Small seams/tooling: `GeodesicIntegrator::adaptiveStep` made public so the
  step policy is directly testable; shared `tests/support/physics_helpers.h`
  (PI + null residual) removes duplication.
- `docs/CONTEXT.md` §2/§6 updated (spec §Further Notes requires it); ticket
  0009 marked `Status: Done`.

## 📊 Verification
- **Typecheck**: 🟢 `cmake --build build -j` clean (core + app + tests)
- **Test suite**: 🟢 `ctest` 4/4 passed, total ~0.01 s
- **No GL context**: 🟢 tests link `blackhole_core` only; `ldd` shows no
  GL/GLFW/GLEW
- **Deterministic**: 🟢 identical output across repeated runs

## 🔍 Code Review Findings
- **Standards**: No GL/contract violations; `adaptiveStep` visibility change is
  a justified seam. Hard gaps flagged: ticket 0009 marked Done without archiving
  to `docs/archive/tickets/` and without a `CHANGELOG.md` entry (same
  outstanding debt as 0008); several bare tolerances lacked justification.
  Judgement calls: duplicated `PI`/`nullResidual`; a header comment welding the
  production header to the test suite.
- **Spec**: All requested areas covered. Findings: null-drift assertion (`1e-9`)
  was 1000× looser than the ticket's claimed `≲1e-12`; deflection band (15%) was
  loose vs the claimed ~6.4%; the immediate-capture fixture at `r = rs` built an
  inf/NaN `p_r` from `f = 0`; `geodesicRHS` only checked 2 of 8 analytic
  components.
- **Addressed in follow-up**: drift tolerance → `1e-12`; deflection band → 10%;
  fixture moved to `r = 2.01` (keeps `f > 0`); added analytic `dp[1]`/`dp[2]`
  checks; hoisted shared helpers to `tests/support/`.
- **Deferred**: ticket archival + `CHANGELOG.md` (lifecycle close, also pending
  for 0008); whether to keep `adaptiveStep` public.

## 📦 Recent Commits
- (none — ticket 0008 landed as `5083202`; 0009 changes are staged, not committed)

## ⏭️ Follow-ups
- Close per lifecycle for 0008 and 0009: archive tickets, update
  `CHANGELOG.md`, fix any `docs/spec/0002` links if tickets move.
- Next ticket in the milestone: 0010 (headless `--render` observation entry
  point), which unblocks 0011/0012/0013.
