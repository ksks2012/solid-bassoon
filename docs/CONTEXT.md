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
rings and a thin accretion disk.

## 2. Current State (codebase facts)

| Item | State |
|---|---|
| Black hole | **Schwarzschild only.** `spin` is parsed from config but not used; no Kerr support |
| Units | Geometric `G = c = 1`; default `M = 1` so `rs = 2M = 2` |
| Coordinate chart | Boyer–Lindquist `(t, r, θ, φ)`; no chart switch near the horizon |
| Integration | RK4 on 8 coupled ODEs (4 coordinates + 4 covariant momenta), fixed base step with adaptive shrink near the horizon |
| Render path | **GPU**, two-pass: compute shader traces rays into an `RGBA32F` image; fullscreen quad presents it |
| CPU integrator | `GeodesicIntegrator` runs a fixed 20000-step validation trace at startup only; never on the render path |
| Camera | Static observer's local orthonormal tetrad, looking radially inward |
| Background | Procedural multi-layer starfield + Milky Way band + coordinate grid; optional point source for the Einstein ring |
| Accretion disk | Equatorial plane only, geometrically thin, optically thick; the first equatorial crossing is opaque and terminates the ray |
| UI | ImGui control panel mutates runtime params and triggers a re-render; changes are **not** written back to `config.json` |
| Config | `etc/config.json` loaded once at startup; missing keys fall back to struct defaults |
| Logging | `ILogger` interface + spdlog backend, reached through the `GlobalLogger` singleton and `LOG_*` macros |
| Build | CMake ≥ 3.10, C++17, OpenGL 4.3 Core; GLEW loader; ccache |
| Third-party | Vendored under `lib/` (imgui, nlohmann/json, spdlog) — see caveats |
| Tests | None automated; the startup CPU trace is the only correctness check |

## 3. Architecture

```
main.cpp
├─ Config            etc/config.json  -> struct params
├─ Window            GLFW OpenGL 4.3 Core context + GLEW init
├─ cpuValidation()   CPU GeodesicIntegrator — startup sanity trace only
├─ RayTracer
│    Pass 1  raytrace.comp   compute, 16x16 groups, one thread per pixel
│              geodesicRHS -> rk4Step -> termination / disk-hit decision
│              writes RGBA32F output image
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
  single runtime parameter block passed to the GPU. `accretion_disk.h`
  documents the disk model and holds its default constants.
- **app** — `Config` (`config.h/.cpp`) parses JSON; `Window`
  (`window.h/.cpp`) wraps GLFW/GLEW and logs GL/compute limits.
- **logging** — `logger.h` defines `ILogger`/`GlobalLogger`; `spdlog_logger.*`
  is the backend.
- **shaders** — `raytrace.comp` (all shipping physics), `fullscreen.vert`,
  `render.frag`.

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

## 6. Known Gaps & Caveats

- **`lib/` is gitignored and currently absent.** The imgui / nlohmann-json /
  spdlog sources must exist under `lib/` for the build to succeed; they are not
  tracked in git.
- **Kerr spin is not implemented.** `blackhole.spin` is parsed and ignored.
- **CPU/GPU termination thresholds differ.** CPU uses `r ≤ rs·1.01`
  (`HORIZON_EPSILON`), GPU uses `r ≤ rs·1.05`; adaptive-step floors also differ
  (CPU `0.001` vs GPU `0.01` of the base step).
- **Disk default is inside the ISCO.** `config.json` sets `inner_radius = 3.0`
  while ISCO `= 3·rs = 6.0`; the UI flags this as unphysical.
- **`MAX_STEPS` is repurposed on the GPU.** Rays that exhaust the budget near
  the photon sphere are drawn as photon-ring glow; the rest show debug magenta.
  A future refactor should separate the two meanings: a genuinely exhausted
  step budget (an accuracy warning) versus rays that legitimately wound near
  the photon sphere (real photon-ring light).
- **No tone mapping or gamma correction.** The compute shader clamps its linear
  `RGBA32F` output straight to `[0, 1]`, so HDR highlights (e.g. Doppler-boosted
  disk or photon-ring light) are clipped rather than mapped. Candidate P2 work.
- **Software rendering works but is slow.** Mesa llvmpipe is detected and
  warned about; compute dispatch has no GPU acceleration there.
- **No automated tests.**

## 7. Directory Layout

```
solid-bassoon/
├── CMakeLists.txt
├── etc/config.json          # startup configuration
├── shaders/                 # raytrace.comp, fullscreen.vert, render.frag
├── include/{core,rendering,app,logging}/
├── src/{core,rendering,app,logging}/ + main.cpp
├── lib/                     # vendored imgui / nlohmann / spdlog (gitignored)
├── fig/                     # screenshots referenced by the README
└── docs/                    # CONTEXT.md, DEFINITION.md, adr/, ticket/
```
