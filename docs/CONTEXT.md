# CONTEXT — Blackhole Renderer

> Single source of truth for the current architecture and project-wide
> constraints. It consolidates the core context with accepted decisions.
> Glossary: [DEFINITION.md](./DEFINITION.md). Documentation lifecycle:
> [DOCUMENTATION-LIFECYCLE.md](./DOCUMENTATION-LIFECYCLE.md). Feature overview
> and screenshots: [../README.md](../README.md).

## 1. What this is

A real-time renderer of a non-rotating (Schwarzschild) black hole. Each screen
pixel casts one light ray **backwards** from a static observer's camera through
curved spacetime, integrating the null geodesic equations in Hamiltonian form.
The result reproduces gravitational lensing, a photon-sphere shadow, Einstein
rings, a thin accretion disk, and an opt-in relativistic jet with Doppler
beaming.

## 2. Current State (codebase facts)

| Item | State |
|---|---|
| Black hole | **Schwarzschild only.** `spin` is parsed from config but not used; no Kerr support |
| Units | Geometric `G = c = 1`; default `M = 1` so `rs = 2M = 2` |
| Coordinate chart | Boyer–Lindquist `(t, r, θ, φ)`; no chart switch near the horizon |
| Integration | RK4 on 8 coupled ODEs (4 coordinates + 4 covariant momenta), fixed base step with adaptive shrink near the horizon |
| Render path | **GPU**, two-pass: compute shader traces rays into an `RGBA32F` image; fullscreen quad presents it |
| Display mapping | Global tone mapping at the end of the compute shader: exposure → soft-knee highlight roll-off (identity below the knee) → optional gamma. Defaults (exposure 1.0, gamma 1.0) leave the normal range unchanged. `exposure`/`gamma` are runtime params (config + ImGui) |
| CPU integrator | `GeodesicIntegrator` runs a fixed 20000-step validation trace at startup only; never on the render path |
| Camera | Static observer's local orthonormal tetrad, looking radially inward |
| Background | Procedural multi-layer starfield + Milky Way band + coordinate grid; optional point source for the Einstein ring |
| Accretion disk | Equatorial plane only, geometrically thin, optically thick; the first equatorial crossing is opaque and terminates the ray. Default inner edge at the ISCO (`3·rs = 6M`); outer `12M` |
| Relativistic jet | Opt-in (off by default). Bipolar, axisymmetric, **emission-only** (optically thin, additive) solid cone with a transverse Gaussian profile, bounded axial power-law and soft top fade. Doppler beaming from a single constant bulk velocity `β` (`δ^(3+α)`). All parameters live in `config.json` + ImGui |
| UI | ImGui control panel mutates runtime params and triggers a re-render; changes are **not** written back to `config.json` |
| Config | `etc/config.json` loaded once at startup; missing keys fall back to struct defaults |
| Automation | `--render <config.json> --out <file>` renders one frame with a **hidden** window, reads back the post-tone-map `RGBA32F` texture (`RayTracer::readback` → `glGetTexImage`), and writes raw little-endian `float32` + a `<file>.meta` shape sidecar — the observation seam for GPU regression. `--probe <file>` dispatches `shaders/probe.comp` over a fixed input table and writes the kernel outputs the same way. No GL 4.3 compute → exit 2 (`skipped`), never a false pass. `--bench <config> --warmup N --frames M` runs a hidden-window timed loop and prints one CSV row (`median, p95, mean, min, fps, renderer, steps_per_ray, …`) tagged with `GL_RENDERER` — the observation seam for performance regression (`runBench`, `core/bench_stats`) |
| Logging | `ILogger` interface + spdlog backend, reached through the `GlobalLogger` singleton and `LOG_*` macros |
| Build | CMake ≥ 3.10, C++17, OpenGL 4.3 Core; GLEW loader; ccache |
| Third-party | Vendored under `lib/` (imgui, nlohmann/json, spdlog) — see caveats |
| Tests | CPU unit tests for metric / integrator / config / **emission reference** / **bench statistics** (`tests/`) plus GPU checks under CTest: a headless-render determinism check, golden regression against five committed float32 references (`tests/reference/`, including a disk on/off locality invariant), a **GPU↔CPU probe cross-validation** of all four emission kernels, a full-render finite/non-negative/tone-map-ceiling invariant, and a **performance gate** against the committed baseline — 14 tests. The unit layer needs no GL context; GPU checks self-report `skipped` (exit 2) when no 4.3 compute context exists. Performance is automated by `bench/gate.py` (CTest `test_perf_gate`): correctness-normalized, ±10% band on the median, budgets at 512×512 — see [bench/README.md](../bench/README.md) |

## 3. Architecture

```
main.cpp
├─ CLI               no args → interactive; `--render <cfg> --out <file>` → headless
│                     `--probe <file>` → hidden 64x64 context, one kernel-probe dispatch
│                     `--bench <cfg> --warmup N --frames M` → timed loop, one CSV row
├─ Config            etc/config.json  -> struct params
├─ buildRayTracerParams(config)  config -> RayTracerParams (shared by both paths)
├─ Window            GLFW OpenGL 4.3 Core context + GLEW init (visible or hidden)
├─ cpuValidation()   CPU GeodesicIntegrator — startup sanity trace only (interactive)
├─ [headless]        hidden window → one compute dispatch → RayTracer::readback → file
├─ [probe]           hidden window → RayTracer::runProbe → probe.comp SSBO → file
├─ [bench]           hidden window → warm-up discarded → N timed dispatches
│                      (+ a stats pass with atomic counters on, same N)
│                      → core/bench_stats::summarize → CSV header + row
├─ RayTracer         (interactive path)
│    Pass 1  raytrace.comp   compute, 16x16 groups, one thread per pixel
│              #include "physics.glsl"  pure kernels, parameters passed in
│              loop: rk4Step -> accumulateJet -> disk-hit test -> termination
│              terminal colour (captured/disk/sky) + jetAccum
│              applyToneMap (exposure + soft-knee + gamma) -> RGBA32F image
│              benchCounters SSBO (binding 3) <- atomicAdd per ray/step,
│              only when u_statsEnabled — costs ~+110-150% of frame time
│    Pass 2  fullscreen.vert + render.frag   sample the image to the screen
│    dispatchTimed()  GL_TIME_ELAPSED around glDispatchCompute (bench only)
└─ ImGui overlay
```

Layers:

- **core** — `SchwarzschildMetric` (`metric.h/.cpp`): metric components, their
  inverses, the geodesic RHS and null-ray momentum initialisation.
  `GeodesicIntegrator` (`geodesic_integrator.h/.cpp`): RK4, adaptive step,
  termination, `TraceResult`.
- **rendering** — `RayTracer` (`ray_tracer.h/.cpp`): owns the compute program,
  the `RGBA32F` texture and the fullscreen quad. `RayTracerParams` is the
  single runtime parameter block passed to the GPU (black hole, camera,
  integration, disk, point source, **jet**, display). `accretion_disk.h`
  documents the disk model and holds its default constants; the jet parameter
  block is `JetParams`. `loadShaderSource` expands `#include "..."` textually
  (depth-limited) so `raytrace.comp` can pull in `physics.glsl`; `runProbe`
  dispatches `probe.comp` over an input SSBO and reads the output SSBO back.
- **app** — `Config` (`config.h/.cpp`) parses JSON; `Window`
  (`window.h/.cpp`) wraps GLFW/GLEW and logs GL/compute limits (visible by
  default, `visible=false` for headless); `render_params.*` holds the shared
  config→`RayTracerParams` translation; `headless.*` implements the `--render`
  and `--probe` entry points and raw-float output.
- **logging** — `logger.h` defines `ILogger`/`GlobalLogger`; `spdlog_logger.*`
  is the backend.
- **shaders** — `raytrace.comp` (the shipping render), `physics.glsl` (pure
  emission kernels + `PI`, no uniforms), `probe.comp` (kernel probe),
  `fullscreen.vert`, `render.frag`.

Build targets (see [adr/0005](./adr/0005-extract-blackhole-core.md)):

- **`blackhole_core`** — a **GL-free static library**: `core` + `app/config` +
  `logging`. Must not include or link GL/GLFW/ImGui. Test targets link this
  library **only**.
- **`BlackholeRenderer`** — the executable: `main`, `app/window`,
  `app/render_params`, `app/headless`, `rendering`, vendored ImGui; links
  `blackhole_core` plus OpenGL/GLFW/GLEW.

## 4. Physics & sign conventions (non-negotiable)

1. **Covariant momenta** are used throughout: state `x = (t, r, θ, φ)`,
   momenta `p = (p_t, p_r, p_θ, p_φ)`. `p_t` and `p_φ` are conserved Killing
   quantities (`p_t = -E`, `p_φ = L`).
2. Rays satisfy the **null condition** `g^{μν} p_μ p_ν = 0`. The camera sets
   `E = -p_t = 1`; the remaining momenta come from the observer tetrad so the
   condition holds by construction.
3. Rays are traced **backwards**, from the camera toward the scene.
4. **Termination**: `CAPTURED` when `r ≤ rs·(1+ε)`; `ESCAPED` when
   `r > abort_distance`; `MAX_STEPS` when the step budget runs out.
5. **Disk crossing** is detected by a sign change of `(θ − π/2)` between two
   steps, located by linear interpolation in `r` and `φ`. The disk is opaque:
   the first in-bounds crossing terminates the ray.
6. **Disk emission is a simplified model.** It multiplies the temperature
   profile by a gravitational-redshift factor `√f(r)` and a Doppler factor
   `δ³` built from a non-relativistic Keplerian line-of-sight velocity — an
   approximation, not a full relativistic radiative-transfer treatment. The
   two effects are combined multiplicatively and clamped to `[0, 5]`.
7. **The jet is emission-only and optically thin.** Per geodesic step it adds
   `j · ds` with the local proper length `ds = h_eff / √f`; the total is added
   onto every terminal colour. It never absorbs or occludes background light
   (see [adr/0003](./adr/0003-emission-only-jet.md)).
8. **Jet beaming is a single constant bulk velocity.** `v = sign(z)·β·ẑ`, with
   gain `δ^(3+α)` from the static-local-frame Doppler factor; `β = 0` is
   unbeamed. The emitter → observer direction is the reverse of the backward
   march (`−n̂`) — the sign trap (see [adr/0004](./adr/0004-jet-constant-bulk-velocity-beaming.md)).

## 5. Contracts & conventions

1. **Two mirrored metric implementations.** The geodesic RHS, adaptive step
   and camera tetrad exist twice: once in C++ (`metric.cpp`,
   `geodesic_integrator.cpp`) and once in GLSL (`raytrace.comp`). They are kept
   in sync by hand — a physics change must be applied to **both**.
2. **Header/source separation.** Headers declare types and inline accessors;
   non-trivial bodies live in `.cpp` (`metric.cpp`, `geodesic_integrator.cpp`).
3. **One shipping render entry point, plus automation entry points.** The GPU
   path is the only shipping renderer; the CPU integrator exists to validate the
   geodesic math, not to draw. `--render` (one dispatch + texture readback),
   `--probe` and `--bench` are non-interactive entry points into the *same*
   `RayTracer` (hidden window) used by regression tests — they add no second
   rendering implementation. `--bench` times only what `render()` times: its
   uniforms come from the shared `setComputeUniforms()`, so the bench and the
   shipping path cannot drift apart.
4. **Re-render only on change.** `needsRender` gates the expensive compute
   dispatch; unchanged frames reuse the texture via `drawLastFrame()`.
5. **Run from the repository root.** Shader paths (`shaders/...`) and the config
   path (`etc/config.json`) are resolved relative to the working directory.
6. **Per-pixel exactness.** The output texture uses `NEAREST` filtering — no
   interpolation between neighbouring ray results.
7. `config.json` is startup-only; all live tuning happens through
   `RayTracerParams` and the ImGui panel.
8. **Emission physics is pure and mirrored, but separate from the geodesic
   mirror.** The disk, jet-density, jet-beaming and tone-map kernels live in
   `shaders/physics.glsl` as pure functions that read no uniforms — parameters
   are passed at the call site. They are *not* part of the mirrored metric
   implementation (§5.1), which is scoped to the geodesic RHS, adaptive step
   and camera tetrad. Their second mirror is the double-precision C++
   reimplementation in `core/emission_reference.{h,cpp}`, and the two are kept
   in sync by construction and by measurement: hand-derived literals in
   `tests/unit/emission_reference_test.cpp`, and the 48 shared probe records
   (`core/probe_cases.h`) compared GPU-vs-CPU by `tests/gpu/probe_check.py`.
   A change to one mirror without the other fails the suite.

## 6. Known Gaps & Caveats

- **`lib/` is gitignored and currently absent.** The imgui / nlohmann-json /
  spdlog sources must exist under `lib/` for the build to succeed; they are not
  tracked in git.
- **Kerr spin is not implemented.** `blackhole.spin` is parsed and ignored.
- **CPU/GPU termination thresholds differ.** CPU uses `r ≤ rs·1.01`
  (`HORIZON_EPSILON`), GPU uses `r ≤ rs·1.05`; adaptive-step floors also differ
  (CPU `0.001` vs GPU `0.01` of the base step).
- **`MAX_STEPS` is repurposed on the GPU.** Rays that exhaust the budget near
  the photon sphere are drawn as photon-ring glow; the rest show debug magenta.
  A future refactor should separate the two meanings: a genuinely exhausted
  step budget (an accuracy warning) versus rays that legitimately wound near
  the photon sphere (real photon-ring light).
- **Software rendering works but is slow.** Mesa llvmpipe is detected and
  warned about; compute dispatch has no GPU acceleration there.
- **The jet is emission-only.** No self-absorption, optical depth, or
  absorption of background light; a full radiative-transfer treatment is
  deferred (see [adr/0003](./adr/0003-emission-only-jet.md)).
- **Jet beaming uses a constant `β` and a simplified `n̂`.** The bulk velocity
  does not accelerate with `z`, and the camera's momentum convention makes the
  spec's `n̂` evaluate to `f·`(unit direction) rather than a unit vector,
  slightly under-scaling beaming near the jet base. The implementation is
  faithful to the spec formula (see
  [adr/0004](./adr/0004-jet-constant-bulk-velocity-beaming.md)).
- **Testing covers the CPU core, the headless seam, golden regression, and the
  emission kernels.** A testing & performance standard is defined
  ([standards/testing-and-performance.md](./standards/testing-and-performance.md)),
  with an in-progress harness spec
  ([spec/0002](./spec/0002-testing-and-performance-harness.md)). Tickets 0008
  and 0009 landed the `blackhole_core` split and CPU unit tests; ticket 0010
  landed the headless `--render` + `RayTracer::readback` observation seam and a
  determinism check; ticket 0011 landed golden regression — five committed
  float32 references under `tests/reference/`, a comparator self-test, and a
  disk on/off locality invariant, all registered in CTest
  (`tests/gpu/regression_check.py`). Rendering is byte-deterministic here
  (`max_abs = 0`), and `jetoff` is byte-identical to the pre-jet baseline.
  Ticket 0012 extracted the emission kernels into `shaders/physics.glsl`,
  pinned them with hand-derived literal tests
  (`tests/unit/emission_reference_test.cpp`), and added the GPU↔CPU probe
  cross-validation (`tests/gpu/probe_check.py`, `--probe`) plus a full-render
  finite/non-negative/ceiling invariant (`tests/gpu/no_nan_check.py`).
  Ticket 0013 landed the performance layer: `--bench` (a `GL_TIME_ELAPSED`
  timed loop + a stats pass), `core/bench_stats` (median/p95/mean/min/fps with
  sample hygiene), the five-case configuration matrix, the committed
  `bench/baseline.json` with its budgets, and the correctness-normalized gate
  `bench/gate.py` (±10% band, config/protocol drift, hardware-mismatch skip, skip vs
  fail) registered as `test_perf_gate`, which CTest gates behind a
  `correctness` fixture — it runs only while the rest of the suite is green. Measured on the reference GPU: 7.62 ms at 512×512 vs a
  12 ms budget, jet marginal 1.4% vs 15%. **The last gap in this spec is
  ticket 0014** (running the suite in CI; locally it is 14/14 green).

## 7. Directory Layout

```
solid-bassoon/
├── CMakeLists.txt
├── .context/                # active session state (gitignored)
├── etc/config.json          # startup configuration
├── shaders/                 # raytrace.comp, physics.glsl, probe.comp, fullscreen.vert, render.frag
├── include/{core,rendering,app,logging}/
├── src/{core,rendering,app,logging}/ + main.cpp
├── tests/                   # CTest suite; links blackhole_core only
├── bench/                   # performance gate: gate.py, baseline.json, README.md
├── lib/                     # vendored imgui / nlohmann / spdlog (gitignored)
├── fig/                     # screenshots + acceptance golden images
└── docs/
    ├── CONTEXT.md           # single source of truth (this file)
    ├── DEFINITION.md        # domain glossary
    ├── DOCUMENTATION-LIFECYCLE.md
    ├── CHANGELOG.md         # milestone / version history
    ├── adr/                 # architecture decision records (never deleted)
    ├── spec/                # feature specs (intent); not archived
    ├── ticket/              # active tracer-bullet work units
    ├── standards/           # development & technical standards
    └── archive/
        ├── tickets/         # completed tickets
        └── summaries/       # completed execution summaries
```

Document kinds and their lifecycles are defined in
[DOCUMENTATION-LIFECYCLE.md](./DOCUMENTATION-LIFECYCLE.md): tickets decompose a
spec, a spec references ADRs, and ADRs are superseded rather than rewritten.

## 8. Accepted Decisions (ADRs)

| ADR | Decision | Status |
|-----|----------|--------|
| [0001](./adr/0001-global-tone-mapping.md) | Global tone mapping with a soft-knee highlight roll-off | Accepted |
| [0002](./adr/0002-disk-inner-radius-at-isco.md) | Ship the disk inner radius at the ISCO | Accepted |
| [0003](./adr/0003-emission-only-jet.md) | Emission-only, optically-thin relativistic jet | Accepted |
| [0004](./adr/0004-jet-constant-bulk-velocity-beaming.md) | Model jet beaming with a constant bulk velocity | Accepted |
| [0005](./adr/0005-extract-blackhole-core.md) | Extract a GL-free `blackhole_core` library for testability | Accepted |

Superseded ADRs are kept for history; only Accepted ADRs are reflected above.
