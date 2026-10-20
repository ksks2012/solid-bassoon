# Execution Summary: 0013-benchmark-baseline

- **Date**: 2026-10-20
- **Spec/ADR Path**: `docs/spec/0002-testing-and-performance-harness.md`
- **Ticket Path**: `docs/archive/tickets/0013-benchmark-baseline.md`
- **Branch**: `main`

---

## 📌 Overview
- **Commits**: `1` commit
- **Files Changed**: `20` files (`+2342` / `-26`)

## 📝 Key Changes
* **Measurement seam**: `--bench <config> --warmup N --frames M` runs a hidden-window warm-up + timed loop through `RayTracer::dispatchTimed` (`GL_TIME_ELAPSED` around the compute dispatch only) and publishes one CSV row from the new `core/bench_stats.{h,cpp}` (median / p95 / mean / min / fps, non-finite and non-positive samples dropped). The RK4 diagnostic (`steps_per_ray`) comes from a per-ray/per-step `atomicAdd` SSBO in `raytrace.comp`, measured in its own pass at **+110–150 % of frame time** and therefore excluded from the gated timing.
* **Gate + committed baseline**: `bench/gate.py` renders every case through the ticket-0012 invariant *before* timing it, then applies the ±10 % band (both directions), the reference-GPU budgets (12 ms full frame at 512×512, jet marginal ≤ 15 %) and config/protocol/hardware-drift checks against `bench/baseline.json`. Recorded on the RTX 4070: **7.62 ms median (64 % of budget), jet marginal 1.4 %**, over a five-case matrix (resolution / `max_steps` / jet / disk) documented in `bench/README.md`.
* **Policy stays out of the measurement**: `--update --reason …` re-records numbers only — tolerance, budgets and every unrelated baseline key carry over verbatim, so the command that measures a number cannot redefine the rule it is judged by. Verified live: exit **0** in band, **1** on a tampered baseline (+36 % regression), **2** on hardware the baseline does not name.
* **Criterion 7 made structural**: `test_perf_gate` requires a `correctness` CTest fixture provided by every other test, so a failing correctness test leaves the gate *Not Run*; `ctest -R test_perf_gate` pulls the whole correctness suite in with it.

## 🧪 Verification
- **Typecheck**: 🟢 Pass — `cmake --build build -j` clean (0 errors/warnings)
- **Tests**: 🟢 Pass — **14/14** via `GALLIUM_DRIVER=d3d12 ctest --test-dir build --output-on-failure` (17.4 s); golden regression still reports `max_abs = 0` on all five scenes after the shader gained the counters. Gate self-tests were mutation-checked (widened band, suppressed verdicts, skip-masks-failure, short CSV row, dropped environment finding, dropped jet rule → all detected).

## 🔍 Code Review Findings
- **Standards**: No hard violations of `docs/standards/testing-and-performance.md` or CONTEXT §5. Six judgement-call smells reported: **four actioned** — the cross-language CSV header literal is now pinned by *both* tests, the four sampling loops collapsed into one `sampleFrames()` helper, `main()` and `evaluate_case()` now share one `split_findings()` source, `statsQuery_` renamed `timeQuery_` (it times every dispatch); **two accepted as-is** — the deliberate `tests/gpu` import (shared harness reuse) and eager creation of the bench resources in `init()`.
- **Spec**: One partial requirement (criterion 7, fixed by the fixture), four benign scope additions (`--report`, `--tolerance`, protocol-drift detection, the counter-wrap warning), and three questionable implementations — **all three fixed**: a wrong-GPU run now reports `skip` and exits 2 instead of failing, a jet more than 10 % *faster* than no jet is flagged as a suspicious improvement, and `--update` warns when recording on different hardware while carrying the budgets over unchanged.

## 📦 Recent Commits
- `851d6d0` feat(tests): add --bench, a committed baseline and a performance gate
