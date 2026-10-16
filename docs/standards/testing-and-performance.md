# Testing & Performance Standards

> Development standard for **how this project is verified** and **how its
> performance is measured**. It defines the testable seams, the test layers,
> the tooling, and the benchmark protocol. It is a living standard (see
> [DOCUMENTATION-LIFECYCLE.md](../DOCUMENTATION-LIFECYCLE.md)); architecture
> decisions it depends on are recorded as ADRs.

Related: [CONTEXT.md](../CONTEXT.md) (contracts §5, gaps §6),
[DEFINITION.md](../DEFINITION.md), [CHANGELOG.md](../CHANGELOG.md).

**Status:** Accepted — target standard. The automation it describes is **not
yet implemented**; §7 is the delivery plan.

---

## 1. What we are testing, and the threats to validity

The product is a GPU ray tracer whose physics is **duplicated** between C++
(`metric.cpp`, `geodesic_integrator.cpp`) and GLSL (`raytrace.comp`) — see
CONTEXT §5.1. Emission physics (disk Doppler, jet density/beaming, tone map)
exists **only** in GLSL (CONTEXT §5.8). Verification must therefore cover:

- **Math correctness** of the CPU core (cheap, exact, fast).
- **GPU/GLSL parity** with the CPU reference (the mirrored-metric contract).
- **GPU-only emission physics**, which has no CPU twin.
- **Visual regressions** in the composed image.
- **Performance**, without silently trading correctness for speed.

Threats to validity and their mitigations:

| Threat | Mitigation |
|---|---|
| GPU float32 ≠ CPU double; driver/ISA differences | compare with **tolerances**, not bit-exactness, except where a true invariant is expected |
| GPU nondeterminism across runs | render the same scene twice and require identical `RGBA32F` output (the path is deterministic — no atomics on the render path) |
| Thermal/clock throttling, boost variance | warm-up frames, report **median & p95**, repeat runs |
| Software renderer (llvmpipe) differs numerically | keep a **separate slow reference set**, tagged by `GL_RENDERER` |
| Golden images rot as intended visuals evolve | goldens are **ticket-scoped** artifacts; changes require an explicit, reviewed update (see §5.4) |

---

## 2. Testability seams

The single most important enabler: **the GL-free core must be linkable without
OpenGL**, and **the GPU render must be scriptable**.

| # | Layer | Seam | Test kind |
|---|---|---|---|
| 1 | `SchwarzschildMetric` | pure functions returning `double` | CPU unit |
| 2 | `GeodesicIntegrator` | `TraceResult` for a given `State` | CPU unit |
| 3 | `Config` | `loadFromFile()` / `ConfigError` | CPU unit |
| 4 | Shader emission kernels | `jetDensity`, `jetBeamFactor`, `diskEmission`, tone map | GPU function probe |
| 5 | Full render | `RayTracer` output `RGBA32F` texture | headless render + golden |
| 6 | CPU vs GPU classification | ray `(α, β)` → terminal status | cross-validation |
| 7 | Performance | compute dispatch time | headless benchmark |

### 2.1 Required refactor — extract a core library

Today every source is globbed into one executable (`CMakeLists.txt:41-64`).
Split it:

```cmake
# linkable, no GL/GLFW/ImGui
add_library(blackhole_core
    src/core/metric.cpp
    src/core/geodesic_integrator.cpp
    src/app/config.cpp
    src/logging/spdlog_logger.cpp
)
target_include_directories(blackhole_core PUBLIC include ${NLOHMANN_JSON_INCLUDE_DIRS} ${SPDLOG_INCLUDE_DIRS} ${IMGUI_INCLUDE_DIRS})

# the app keeps GL/GLFW/ImGui/window/rendering
add_executable(BlackholeRenderer src/main.cpp src/app/window.cpp src/rendering/ray_tracer.cpp ${IMGUI_SOURCES})
target_link_libraries(BlackholeRenderer PRIVATE blackhole_core OpenGL::GL glfw GLEW::GLEW)
```

Tests link `blackhole_core` only.

> **Do this first.** The split is the **blocking prerequisite** for test layers
> 1–3 (§3.1) and for the headless CLI (§2.2) — nothing below can be wired into
> CTest until it lands. It is an architecture change → record **ADR-0005** and
> update CONTEXT §3/§7 when done.

### 2.2 Required capability — headless CLI mode

Add subcommands to the executable so the GPU path is automatable (a hidden
GLFW window reuses `Window`; no EGL dependency):

```
BlackholeRenderer --render <config.json> --out <file.ppm>            # one frame, read back, exit
BlackholeRenderer --bench  <config.json> --warmup N --frames M       # timing, CSV to stdout
```

`--render` reads back `RayTracer::outputTexture_` via
`glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, …)` — the **post-tone-map
`RGBA32F`** image, not the window back-buffer. This is the deterministic seam:
`NEAREST` filtering, no present/sRGB ambiguity (CONTEXT §5.6). Add a thin
`RayTracer::readback(std::vector<float>&)` accessor.

> **CI caution.** A hidden GLFW window still needs a real GL 4.3 + compute
> context. On a display-less runner without a GPU (notably WSL2 + D3D12), the
> app must **fail loudly or report `skipped`** — never emit a green result for
> a render it never produced (§5). "No context available" is not a pass.

---

## 3. Test layers

Test pyramid adapted to a GPU product: cheap exact CPU tests at the base,
expensive image/perf tests at the top.

```
        perf budget        (1: environment-sensitive, gated loosely)
      cross-validation     (few: mirrored-metric parity)
     image regression      (dozens: golden + invariance)
   GPU physics probes      (emission kernels, invariants)
 CPU unit tests            (many: exact, milliseconds)
```

### 3.1 CPU unit tests (no GL)

**`SchwarzschildMetric`**
- `g_μν · g^νλ = δ`: `g_tt·g_tt_inv`, `g_rr·g_rr_inv`, `g_thth·g_thth_inv`, `g_phph·g_phph_inv` == 1 (within `1e-12`).
- Constants: `rs == 2M`, `photonSphere == 3M`, `iscoRadius == 6M`.
- `geodesicRHS` conserves `p_t` and `p_φ` (`dp[0]==0`, `dp[3]==0` exactly).
- `computeNullPr` yields a **null** vector: `g^{μν} p_μ p_ν ≈ 0` (≤ `1e-12`).
- Flat-space limit: for large `r`, RHS approaches the straight-line (undeflected) RHS.
- `insideHorizon` boundary at `rs·(1+ε)`.

**`GeodesicIntegrator`**
- **Null-condition drift**: along a traced ray, `|g^{μν}p_μp_ν|` stays small — the key accuracy invariant.
- **Conservation drift**: `p_t`, `p_φ` constant along the integration.
- **Classification**: radial ingoing → `CAPTURED`; large impact parameter → `ESCAPED`; tiny `nmax` → `MAX_STEPS`.
- **Weak-field deflection**: total deflection of a large-`b` ray ≈ `4M/b` within a few %.
- **RK4 order**: error scales ≈ `h⁴` under step refinement (Richardson).
- **`adaptiveStep`**: monotone in `r`, floored at `0.001·h`, capped at `h`.

**`Config`**
- defaults when a key is absent; overrides applied when present.
- unknown keys ignored; missing file → `ConfigError`; malformed JSON → `ConfigError`.
- round-trip against `etc/config.json`.

Framework: **CTest** + a small committed header (`tests/check.h`) with
`CHECK`, `CHECK_CLOSE`, `REQUIRE` macros and a `main` per test file. No external
dependency (the vendored `lib/` is gitignored, so a test framework must live in
`tests/`, not `lib/`).

### 3.2 GPU physics probes (emission kernels)

The emission physics has no CPU twin (CONTEXT §5.8), so it needs its own
harness. Two complementary techniques:

1. **Invariant tests on the full image** (no extra shader): no `NaN`/`Inf`; all
   channels finite and `≥ 0`; output clamped to the tone-map ceiling.
2. **Function probes**: a small probe compute shader (`tests/gpu/probe.comp`)
   includes the same pure functions by refactoring them into
   `shaders/physics.glsl` (included by both `raytrace.comp` and `probe.comp`).
   Feed fixed inputs through an SSBO, read back, and compare against an
   **independent CPU reimplementation** (double precision):
   - `jetDensity` → profile (cone vs parabola exponent), Gaussian transverse
     falloff, axial power-law, soft top fade, `max_height` cut.
   - `jetBeamFactor` → `δ^(3+α)`; `β=0` ⇒ factor exactly the unbeamed value;
     monotone in `|β|`; approaching component brightens, receding dims.
   - `diskEmission` → `T(r) ∝ r^{-3/4}·[1-√(r_in/r)]^{1/4}` shape.
   - tone map → **identity below the knee** (`0.8`); monotone and `≤ 1.0` above.

> **Recommended, not optional.** Extracting the pure kernels into
> `shaders/physics.glsl` (included by both `raytrace.comp` and `probe.comp`)
> removes GLSL duplication and creates the **only** testable surface for
> GPU-only emission physics. Treat it as high priority and ADR-worthy on its
> own.

### 3.3 Image regression (golden)

`--render` on canned configs, compare to references in `tests/reference/`.
Migrate the existing acceptance goldens
(`fig/ticket-0007-{inclined,edgeon,nearaxis,jetoff}.png`,
`fig/ticket-0001.png`, `fig/ticket-0004.png`) into this set.

**Reference format.** Store each reference twice: a **raw little-endian
`float32`** buffer (`.f32`, `W·H·4` channels `RGBA`) used for comparison, and
an **8-bit `P6` PPM** for human viewing. `compare.py` runs on the `.f32`
buffers, never on the quantised PPM, so no precision is lost to export. Record
the expected resolution and channel count in a sidecar so endianness/stride
mistakes fail loudly instead of silently.

Comparison reports three numbers (computed by `tests/gpu/compare.py`, stdlib
only): **max abs diff**, **mean abs diff**, **% pixels above ε**. Tolerances
are per-scenario: near-exact for invariants, looser (`~1e-3`) where float order
differs.

Scenario matrix:

| Scenario | Config | Primary assertion |
|---|---|---|
| jet off | `enabled:false` | **byte-identical** to the pre-jet baseline (airtight regression — jet must not perturb the existing image) |
| jet on, inclined | preset | golden match; front lobe brighter than back |
| jet on, edge-on | inclination 90° | golden match; `β=0` symmetric top/bottom |
| jet on, near-axis | small inclination | golden match |
| beaming asymmetry | `β=+0.95` vs `-0.95` | approaching lobe brighter; monotone in `β` |
| disk off vs on | toggle | differences **only** inside the disk footprint |
| disk ISCO | inner 6 vs 5 | deltas localized to `r<6M` |
| Einstein ring | point source preset | ring present (radial intensity peak) |
| tone-map extremes | knee `0.8` | identity below knee; ceiling `≤1.0` |
| determinism | any | render twice ⇒ identical buffer |
| no-NaN | all | all channels finite, `≥0` |

The `jet off` byte-identity check and the `disk off/on` localized-delta check
are the two highest-value, lowest-cost regressions — both were used ad hoc
during P2.1 and should be automated.

### 3.4 CPU ↔ GPU cross-validation

The CPU integrator exists to validate the geodesic math (CONTEXT §5.3) but is
only run once at startup today (`main.cpp:135`). Formalize it:

- A table of `(α, β)` rays with the status the CPU `GeodesicIntegrator` assigns.
- The GPU classifies each pixel; compare the terminal class at the
  corresponding ray.
- Because CPU/GPU termination thresholds intentionally differ (CONTEXT §6:
  `ε` 0.01 vs 0.05, step floors 0.001 vs 0.01), assert **agreement on
  unambiguous rays** (far-capture, far-escape) and allow a documented ambiguity
  band near the photon sphere.

---

## 4. Layout and commands

```
tests/
├── check.h                 # minimal assertion harness (committed)
├── unit/
│   ├── test_metric.cpp
│   ├── test_integrator.cpp
│   └── test_config.cpp
├── gpu/
│   ├── probe.comp          # emission-kernel probe
│   ├── compare.py          # PPM diff, stdlib only
│   └── run_gpu_tests.sh
├── reference/              # golden PPMs (migrated from fig/)
└── bench/
    └── run_bench.sh
bench/
└── baseline.json           # committed perf baseline (see §6)
```

Commands:

```
cmake -S . -B build && cmake --build build -j
ctest --test-dir build --output-on-failure          # unit + gpu tests
./bin/BlackholeRenderer --render etc/config.json --out /tmp/frame.ppm
./bin/BlackholeRenderer --bench  etc/config.json --warmup 30 --frames 300
```

---

## 5. CI gating

| Gate | Runs on | Blocks on |
|---|---|---|
| Build / typecheck | every push | compile error, warning-as-error if adopted |
| Unit tests | every push | any failure (exact, fast, no GPU) |
| GPU image tests | GPU runner only | golden tolerance, invariants |
| Perf budget | GPU runner, non-blocking by default | flag if > threshold (see §6.3) |

**GPU-runner caveat.** This project's GPU path runs on WSL2 + D3D12 (no
`/dev/dri`). CI must either use a self-hosted WSL2 runner or gracefully **skip**
layer-5 tests and run unit tests + image invariants on llvmpipe against a
separate slow reference. Never let a missing GPU silently turn into a green
"pass" — report **skipped**, not passed.

### 5.1 Golden-update policy

Goldens change only by explicit intent. Update procedure: regenerate, inspect
the diff image, record the reason in the ticket/summary, and replace the
reference in the **same** commit as the code change. A golden change without a
code change is a bug.

---

## 6. Performance evaluation

### 6.1 What "performance" means here

Primary criterion: **compute-dispatch frame time** at a fixed resolution and
step budget — the cost of one full ray-trace, independent of UI and vsync.
Secondary criteria: present/quad time and end-to-end frame time. All perf
numbers are **correctness-normalized**: a faster render that fails the golden
tests is not an improvement.

- **GPU time**: `glBeginQuery(GL_TIME_ELAPSED, q)` around the compute dispatch,
  `glGetQueryObjectui64v` — excludes CPU/ImGui/present, immune to vsync.
- **Wall time**: `std::chrono` around `RayTracer::render()`, reported for
  context.
- **Renderer tag**: every result is tagged with `GL_RENDERER` (llvmpipe vs
  RTX 4070 numbers are not comparable).

### 6.2 Benchmark protocol

`--bench` renders `warmup` frames (discard), then `frames` measured frames and
emits CSV: `median_ms, p95_ms, mean_ms, min_ms, fps, renderer, width, height,
max_steps, jet, disk`. Rules:

- Discard warm-up (shader JIT, power-state ramp).
- Report **median and p95**, never a lone mean.
- Pin the GPU power/clock state where possible; note it when it cannot be pinned.
- Repeat and check run-to-run variance; a change smaller than the noise floor
  is **not** a regression.

### 6.3 Baseline and regression gate

Store a committed baseline (`bench/baseline.json`) recording the reference
machine, driver, resolution, and step budget. The perf gate compares median
dispatch time and flags:

- **regression**: median worse by > 10%, or
- **improvement** (also worth a flag): median better by > 10%, which must be
  accompanied by a passing golden suite.

Absolute budgets (guideline, RTX 4070, 512², 5000 steps): full frame
**≤ ~12 ms**; the jet's **marginal** cost **≤ ~15%** over the jet-off scene.
These match the measured P2.1 baseline (~10 ms/frame; jet +12–14%; see
[CHANGELOG.md](../CHANGELOG.md)).

### 6.4 Configuration matrix

Report a table across a small matrix rather than one number:

| Axis | Values |
|---|---|
| Resolution | 512×512, 1280×720, 1920×1080 |
| `max_steps` | 1000, 5000 |
| Jet | off, on |
| Disk | off, on |

Derived diagnostics worth capturing:

- **Average RK4 steps per ray** (add an SSBO atomic step counter to the compute
  shader; read it back in `--bench`). Cost is linear in average steps, so this
  explains almost all perf movement.
- **Resolution scaling**: confirm cost scales with pixel count.
- **Marginal cost** of jet / disk / higher `max_steps`: measured by toggling one
  axis at a time from a fixed base config.

### 6.5 Reporting artifacts

`bench/results.csv` (raw), a rendered markdown table (human), and the committed
`bench/baseline.json` (gate input). Keep it lean, per the lifecycle: no stale
one-off measurement files.

---

## 7. Rollout plan (to be cut into tickets against a spec)

Because layers 5–7 need the §2 refactors, adopt in this order:

1. **Extract `blackhole_core`** + CTest skeleton (ADR-0005). **Blocking
   prerequisite** for layers 1–3 and the headless CLI.
2. **CPU unit tests** for metric / integrator / config.
3. **Headless `--render`** + `RayTracer::readback`.
4. **Golden harness** + migrate `fig/ticket-0007-*` references; wire the
   jet-off byte-identity and disk-localized-delta invariants.
5. **GPU physics probes** + the `physics.glsl` split + no-NaN invariant.
6. **`--bench`** + `bench/baseline.json` + the performance gate.
7. **CI wiring** with honest skip reporting.

Until this lands, CONTEXT §2 ("Tests: None automated") and §6 ("No automated
tests") remain accurate; update them as each layer is delivered.
