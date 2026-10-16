# 0013 — `--bench` + committed baseline + performance gate

**Spec:** [../spec/0002-testing-and-performance-harness.md](../spec/0002-testing-and-performance-harness.md)

**What to build:** The performance layer: a headless benchmark entry point, a committed baseline, and an explicit correctness-normalized gate.

**Blocked by:** 0010 — needs the headless render entry point.

**Status:** Todo

- [ ] `--bench <config> --warmup N --frames M` renders, discards the warm-up, and emits `median, p95, mean, min, fps, renderer` as CSV.
- [ ] GPU time is measured with a timer query around the compute dispatch and tagged with `GL_RENDERER`.
- [ ] `bench/baseline.json` is committed; the gate flags a regression (or a suspicious improvement) beyond ±10%.
- [ ] The configuration matrix (resolution / `max_steps` / jet / disk) is documented and reportable.
- [ ] Derived diagnostic: average RK4 steps per ray. The per-step SSBO atomic has its own cost — measure and record it, and keep it out of the gated measurement so it does not distort the baseline.
- [ ] Budgets recorded: full frame ≤ ~12 ms and jet marginal cost ≤ ~15% at 512×512 on the reference GPU.
- [ ] Every perf result is published alongside a passing correctness suite.

**See:** [../standards/testing-and-performance.md](../standards/testing-and-performance.md) · [../adr/0005-extract-blackhole-core.md](../adr/0005-extract-blackhole-core.md)
