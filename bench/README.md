# bench/ — the performance gate (ticket 0013)

What is measured, how, and what it is judged against. The rule this directory
implements is [docs/standards/testing-and-performance.md](../docs/standards/testing-and-performance.md) §4:
warm up, report median and a tail percentile (never a lone mean), record the
environment, gate against a committed baseline at an explicit threshold, flag
both regressions *and* suspicious improvements, and publish a result only
alongside passing correctness checks.

```bash
# One measurement, CSV to stdout (run from the repository root):
GALLIUM_DRIVER=d3d12 ./bin/BlackholeRenderer --bench etc/config.json --warmup 5 --frames 20

# The gate — correctness first, then the comparison (CTest: test_perf_gate):
GALLIUM_DRIVER=d3d12 python3 bench/gate.py --renderer ./bin/BlackholeRenderer

# Re-record after an intentional change (policy is preserved, reason is required):
python3 bench/gate.py --renderer ./bin/BlackholeRenderer \
        --update --reason "why these numbers are the new truth"
```

## What `--bench` emits

The CSV header and one row, printed as the last thing the renderer writes, so
`gate.py` can find the header among the log lines and read the row after it:

```
median_ms,p95_ms,mean_ms,min_ms,fps,renderer,steps_per_ray,stats_median_ms,stats_overhead_pct,width,height,max_steps,jet_enabled,disk_enabled,frames,warmup
7.60218,7.71482,7.61216,7.55098,131.541,D3D12 (NVIDIA GeForce RTX 4070),1207.4,15.7573,107.274,512,512,5000,0,1,20,5
```

| Field | Meaning |
|---|---|
| `median_ms`, `p95_ms`, `mean_ms`, `min_ms`, `fps` | Order statistics of the timed samples (`core/bench_stats.h`, unit-tested in `tests/unit/bench_stats_test.cpp`). The gate compares **median**; p95 is the required tail. |
| `renderer` | `GL_RENDERER`, CSV-quoted — the environment the numbers belong to |
| `steps_per_ray` | Derived diagnostic: total RK4 steps ÷ total rays, from the stats pass |
| `stats_median_ms`, `stats_overhead_pct` | The same workload with the counters switched on, and what that costs |
| `width`, `height`, `max_steps`, `jet_enabled`, `disk_enabled` | Configuration-matrix axes of this run |
| `frames`, `warmup` | The sampling protocol; must match the baseline's |

**The timing itself** is a `GL_TIME_ELAPSED` query around the compute dispatch
only — no present pass, no CPU-side uniform upload, no readback — so the number
is GPU time spent tracing the frame. Warm-up frames are dispatched and
discarded first; `fps` is derived from the median.

## Configuration matrix

Every axis the ticket asks to keep reportable is covered by at least one case,
and each case names the config file it comes from (stored in
`bench/baseline.json`):

| Case | Config | Resolution | max_steps | Jet | Disk | Why it is in the matrix |
|---|---|---|---|---|---|---|
| `disk` | `etc/config.json` | 512×512 | 5000 | off | on | The reference frame; carries the ≤12 ms budget |
| `jet_on` | `bench/cases/jet_on.json` | 512×512 | 5000 | **on** | on | Jet axis, and the numerator of the marginal-cost budget |
| `disk_off` | `tests/reference/scenes/diskoff.json` | 512×512 | 5000 | off | **off** | Disk axis |
| `hi_res` | `bench/cases/hi_res.json` | **1024×1024** | 5000 | off | on | Resolution axis (reported, not held to the 512 budget) |
| `low_steps` | `bench/cases/low_steps.json` | 512×512 | **1000** | off | on | Step-budget axis |

`disk` and `jet_on` differ **only** in `relativistic_jet.enabled`, which is what
makes their difference attributable to the jet.

## Budgets (policy, in `bench/baseline.json`)

| Budget | Value | Scope |
|---|---|---|
| Full frame (compute dispatch) | ≤ 12 ms | 512×512 cases, reference GPU |
| Jet marginal cost | ≤ 15 % | `disk` → `jet_on`, from the same timed run |

Both come from spec 0002's performance protocol and are recorded against the
reference GPU (RTX 4070) at 512×512 / 5000 steps. The budgets live in
`baseline.json` as *policy*, and `--update` carries that file's keys over
verbatim — the command that measures a number cannot also redefine the number
it is judged by.

Recorded on 2026-10-20, `D3D12 (NVIDIA GeForce RTX 4070)`, warmup 5 / frames 20:

| Case | median | p95 | vs budget | steps/ray | stats overhead |
|---|---|---|---|---|---|
| `disk` | 7.620 ms | 8.596 ms | **64 % of the 12 ms budget** | 1207.4 | +120 % |
| `jet_on` | 7.725 ms | 8.688 ms | 64 % | 1207.4 | +117 % |
| `disk_off` | 7.298 ms | 8.402 ms | 61 % | 1310.9 | +147 % |
| `hi_res` | 27.241 ms | 27.448 ms | n/a (1024²) | 1207.6 | +141 % |
| `low_steps` | 4.951 ms | 5.723 ms | 41 % | 896.4 | +143 % |

Jet marginal cost: **1.4 %** against the ≤ 15 % budget.

## The RK4 diagnostic and what it costs

`steps_per_ray` is counted *in the shader* with a per-step `atomicAdd` into a
32-bit SSBO (`benchCounters`, binding 3), enabled by `u_statsEnabled`. That
atomic is exact and expensive: every ray in flight contends for one word, and
the measurement above shows it **roughly doubles the frame time** (+117 % to
+147 %). So:

- every timed dispatch runs with the counters **off**, and the gate never sees
  their cost in `median_ms`;
- a separate pass with the same warm-up and frame count measures the counters
  *and* supplies the totals, published as `stats_overhead_pct`.

Two consequences worth knowing:

- The totals are 32-bit, so they wrap above 4.29e9 steps. The theoretical
  worst case is `pixels × max_steps`; `runBench()` logs a warning when a config
  could reach it (`hi_res` does: 1024² × 5000 = 5.37e9). Measured totals stay
  an order of magnitude below (rays terminate early: ~1200 steps/ray), but a
  config whose rays really did run the full budget would need a wider counter.
- Because it is that slow, the diagnostic pass is a *pass*, not a mode.

## Gate rules (`bench/gate.py`)

1. **Correctness first.** Each case is rendered once with `--render` and
   checked for finite / non-negative / inside-the-tone-map-ceiling / non-blank
   (`tests/gpu/no_nan_check.py`) before it is timed, and again before any
   number is *recorded*. No correct number, no published number.
2. **±10 % band on the median**, both directions. A regression fails; an
   improvement beyond the band fails too, labelled *suspicious* — it is as
   likely to be a broken measurement as a real win, and re-baselining is how
   that would be hidden.
3. **Budgets**, at the resolution they were recorded for (512×512 only, plus
   the jet pair) — checked in both directions: over the ≤15% budget fails, and
   a jet more than 10% *faster* than no jet is flagged as a suspicious
   improvement rather than silently passing (jet emission only adds work).
4. **Config drift and protocol drift.** Different `width`/`height`/
   `max_steps`/`jet`/`disk`, or a different `warmup`/`frames`, means the
   baseline and the run describe different work — reported rather than
   compared. (Measured: a `--warmup 1 --frames 3` run reads ~11 % high on
   `hi_res` and ~9 % *low* on `disk`, which is the whole band.)
5. **Environment.** `GL_RENDERER` must match the baseline's. On any other
   hardware there is nothing to judge: the run is measured and printed, the
   per-case verdicts are `skip`, the comparisons are dropped, and the gate
   exits 2 — a timing difference on a different GPU is an incomparable
   environment, not a finding. *Errors* are the exception: a failed correctness
   check or an unusable row does not depend on how fast the machine is, so it
   still exits 1. A skip never becomes a pass.
6. **Exit codes**: 0 pass, 1 fail, 2 skip (`SKIP_RETURN_CODE 2` in CTest).

## Re-recording

`--update` refuses while any *error* exists (a bad frame is never recorded),
requires `--reason` (recorded under `last_update` in the JSON), replaces only
the measured fields, and keeps every other committed key — tolerance, budgets,
and any metadata — untouched. Timing comparisons do not block it: they are
informational while re-recording. Recording on a GPU other than the baseline's
is allowed (that is what `--reason` is for) but is called out on stderr, with
the note that the budgets are policy and still describe the previous GPU.
Changing a budget is a *different* edit: to `baseline.json` itself, in a diff
someone reviews.

## Tests

| Test | Needs GPU | What it pins |
|---|---|---|
| `test_bench_stats` | no | Order statistics and the CSV row contract, including the header string itself (`tests/unit/bench_stats_test.cpp`) |
| `test_bench_gate` | no | Band boundaries, budget scope (both directions), skip/fail rules, CSV parsing, protocol and hardware drift, policy preservation (`bench/test_gate.py`) |
| `test_perf_gate` | yes | The whole loop against the committed baseline; skips honestly without a 4.3 compute context, and only runs once the correctness suite has passed (CTest `correctness` fixture) |

The CSV header is written down twice — in `bench_stats.cpp` and in
`gate.py` — so **both** tests pin the same literal string: renaming a field on
one side fails the other side's test instead of silently re-aligning every
column for every reader.
