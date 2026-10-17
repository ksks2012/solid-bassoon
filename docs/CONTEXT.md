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
| Logging | `ILogger` interface + spdlog backend, reached through the `GlobalLogger` singleton and `LOG_*` macros |
| Build | CMake ≥ 3.10, C++17, OpenGL 4.3 Core; GLEW loader; ccache |
| Third-party | Vendored under `lib/` (imgui, nlohmann/json, spdlog) — see caveats |
| Tests | A CTest skeleton exists (`tests/`, ticket 0008); the startup CPU trace is still the only real correctness check. Coverage pending |

## 3. Architecture

```
main.cpp
├─ Config            etc/config.json  -> struct params
├─ Window            GLFW OpenGL 4.3 Core context + GLEW init
├─ cpuValidation()   CPU GeodesicIntegrator — startup sanity trace only
├─ RayTracer
│    Pass 1  raytrace.comp   compute, 16x16 groups, one thread per pixel
│              loop: rk4Step -> accumulateJet -> disk-hit test -> termination
│              terminal colour (captured/disk/sky) + jetAccum
│              tone map (exposure + soft-knee + gamma) -> writes RGBA32F image
│    Pass 2  fullscreen.vert + render.frag   sample the image to the screen
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
  block is `JetParams`.
- **app** — `Config` (`config.h/.cpp`) parses JSON; `Window`
  (`window.h/.cpp`) wraps GLFW/GLEW and logs GL/compute limits.
- **logging** — `logger.h` defines `ILogger`/`GlobalLogger`; `spdlog_logger.*`
  is the backend.
- **shaders** — `raytrace.comp` (all shipping physics), `fullscreen.vert`,
  `render.frag`.

Build targets (see [adr/0005](./adr/0005-extract-blackhole-core.md)):

- **`blackhole_core`** — a **GL-free static library**: `core` + `app/config` +
  `logging`. Must not include or link GL/GLFW/ImGui. Test targets link this
  library **only**.
- **`BlackholeRenderer`** — the executable: `main`, `app/window`, `rendering`,
  vendored ImGui; links `blackhole_core` plus OpenGL/GLFW/GLEW.

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
3. **One render entry point.** The GPU path is the only shipping renderer;
   the CPU integrator exists to validate the geodesic math, not to draw.
4. **Re-render only on change.** `needsRender` gates the expensive compute
   dispatch; unchanged frames reuse the texture via `drawLastFrame()`.
5. **Run from the repository root.** Shader paths (`shaders/...`) and the config
   path (`etc/config.json`) are resolved relative to the working directory.
6. **Per-pixel exactness.** The output texture uses `NEAREST` filtering — no
   interpolation between neighbouring ray results.
7. `config.json` is startup-only; all live tuning happens through
   `RayTracerParams` and the ImGui panel.
8. **Emission physics is GLSL-only.** The disk Doppler model and the jet
   density/beaming kernel live only in `raytrace.comp` — they are *not* part of
   the mirrored metric implementation (§5.1), which is scoped to the geodesic
   RHS, adaptive step and camera tetrad.

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
- **Testing is only a skeleton so far.** A testing & performance standard is
  defined
  ([standards/testing-and-performance.md](./standards/testing-and-performance.md)),
  with an in-progress harness spec
  ([spec/0002](./spec/0002-testing-and-performance-harness.md)). Ticket 0008
  landed the `blackhole_core` split and a CTest harness, but there is not yet
  meaningful coverage; the startup CPU trace remains the only real check.

## 7. Directory Layout

```
solid-bassoon/
├── CMakeLists.txt
├── .context/                # active session state (gitignored)
├── etc/config.json          # startup configuration
├── shaders/                 # raytrace.comp, fullscreen.vert, render.frag
├── include/{core,rendering,app,logging}/
├── src/{core,rendering,app,logging}/ + main.cpp
├── tests/                   # CTest suite; links blackhole_core only
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
