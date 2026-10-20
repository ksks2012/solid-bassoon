# 0013 — `--bench` + committed baseline + performance gate

**Spec:** [../spec/0002-testing-and-performance-harness.md](../spec/0002-testing-and-performance-harness.md)

**What to build:** The performance layer: a headless benchmark entry point, a committed baseline, and an explicit correctness-normalized gate.

**Blocked by:** 0010 — needs the headless render entry point.

**Status:** Done

- [x] `--bench <config> --warmup N --frames M` renders, discards the warm-up, and emits `median, p95, mean, min, fps, renderer` as CSV.
- [x] GPU time is measured with a timer query around the compute dispatch and tagged with `GL_RENDERER`.
- [x] `bench/baseline.json` is committed; the gate flags a regression (or a suspicious improvement) beyond ±10%.
- [x] The configuration matrix (resolution / `max_steps` / jet / disk) is documented and reportable.
- [x] Derived diagnostic: average RK4 steps per ray. The per-step SSBO atomic has its own cost — measure and record it, and keep it out of the gated measurement so it does not distort the baseline.
- [x] Budgets recorded: full frame ≤ ~12 ms and jet marginal cost ≤ ~15% at 512×512 on the reference GPU.
- [x] Every perf result is published alongside a passing correctness suite.

## How it was done

**Two seams, both written test-first.**

1. `core/bench_stats.{h,cpp}` (in `blackhole_core`, GL-free) — `summarize()`
   turns a vector of GPU-time samples into median/p95/mean/min/fps with the
   definitions pinned by hand-derived literals: nearest-rank p95, mean-of-two
   for an even count, and **non-positive/non-finite samples dropped** rather
   than allowed to corrupt the median (an all-invalid sample yields zeros, so
   the caller sees "no timing" instead of dividing by zero). `csvRow()` +
   `csvHeader()` own the row contract; the test asserts a row always parses to
   the same field count as its header, with a quote-aware counter, because a
   row that drifts from the header parses silently into the wrong columns.
   (`tests/unit/bench_stats_test.cpp`, CTest `test_bench_stats`.)
2. `bench/gate.py` — decision logic at the top of the file (no I/O), runner at
   the bottom; `bench/test_gate.py` exercises the logic on synthetic rows and
   baselines: band boundaries (exactly ±10 % passes, ±10.01 % does not),
   budget scope (512×512 only), the `steps_per_ray`-missing rule, CSV parsing
   out of interleaved logs, refusal of a truncated row, the fail-beats-skip
   rule, baseline validation, protocol drift, and — the load-bearing one —
   that `--update` preserves the tolerance, budgets and any unrelated key in
   `baseline.json`. The suite was mutation-checked: widening the band,
   swallowing violations, letting a skip mask a failure, and accepting a short
   CSV row are each caught.

**The measurement** (`RayTracer::dispatchTimed`) brackets `glDispatchCompute`
with `GL_TIME_ELAPSED`, so `median_ms` is GPU time for tracing the frame — no
present pass, no uniform upload, no readback. Uniform upload moved into
`setComputeUniforms(params, statsEnabled)`, shared by `render()` and the
bench, so the two paths cannot drift; `render()` passes `false`. The diagnostic
pass uses the same warm-up and frame count as the gated loop, so the two
medians are comparable.

**The diagnostic** is a `std430` SSBO (binding 3) of two `uint`s written by
`atomicAdd` under `u_statsEnabled`, once per ray and once per integration step.
It costs a lot — **+107 % to +147 % of frame time** across the matrix, because
every ray contends for one word — which is exactly why it is measured in its
own pass and published as `stats_overhead_pct` while the gated `median_ms` is
taken with the counters off. The 32-bit totals are safe while
`pixels × max_steps < 4.29e9`; `runBench()` logs a warning when a config's
theoretical worst case could wrap (the `hi_res` case does, at 5.37e9, though
its measured total is ~1.3e9).

**The matrix and the numbers.** Five cases, one per axis, all in
`bench/baseline.json`: `disk` (`etc/config.json`), `jet_on` (identical but for
`relativistic_jet.enabled`), `disk_off`, `hi_res` (1024²) and `low_steps`
(1000) — the last two in `bench/cases/`. Recorded 2026-10-20 on
`D3D12 (NVIDIA GeForce RTX 4070)`: `disk` **7.62 ms** median / 8.60 ms p95,
**64 % of the 12 ms budget**; jet marginal cost **1.4 %** against ≤ 15 %;
1207 RK4 steps/ray (896 for `low_steps`, 1311 for `disk_off`, where rays that
miss the disk keep going). See `bench/README.md` for the full table.

**Correctness normalization** is not an afterthought: `gate.py` renders each
case with `--render` and runs the ticket-0012 invariant (finite, non-negative,
inside the tone-mapped ceiling, non-blank) *before* timing it — and refuses
`--update` while any finding exists, so a broken frame can never become a
baseline. Three exit codes end to end, each verified live: 0 in band, 1 on a
tampered baseline (`disk` recorded at 5 ms → `+52.4%` regression), 2 with
`DISPLAY`/`WAYLAND_DISPLAY` unset (no context → skip, not fail).

**Criterion 7 made structural.** "Published alongside a passing correctness
suite" is enforced by CTest itself: every other test provides a `correctness`
fixture and `test_perf_gate` requires it, so a failing correctness test leaves
the gate *Not Run* instead of standing behind timings nobody should trust, and
`ctest -R test_perf_gate` drags the whole correctness suite in with it. Inside
the gate, each case is still rendered and checked before it is timed (§"the
measurement" above).

**Registered in CTest:** `test_bench_stats` (no GPU), `test_bench_gate` (no
GPU), `test_perf_gate` (GPU, `SKIP_RETURN_CODE 2`) — the suite is now 14 tests.
The golden regression still reports `max_abs = 0` on all five scenes after the
shader gained the counters.

**See:** [../standards/testing-and-performance.md](../standards/testing-and-performance.md) · [../../bench/README.md](../../bench/README.md)
