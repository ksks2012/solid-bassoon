# 0002 — Automated Testing & Performance Harness

- **Status:** Accepted · `ready-for-agent`
- **Date:** 2026-10-16
- **Scope:** Introduce a layered automated test harness and a performance benchmark for the renderer, per [../standards/testing-and-performance.md](../standards/testing-and-performance.md).
- **References:** [../CONTEXT.md](../CONTEXT.md), [../DEFINITION.md](../DEFINITION.md), [../DOCUMENTATION-LIFECYCLE.md](../DOCUMENTATION-LIFECYCLE.md), [../standards/testing-and-performance.md](../standards/testing-and-performance.md) · ADRs: [0005](../adr/0005-extract-blackhole-core.md)

---

## Problem Statement

The renderer has **no automated tests** (CONTEXT §2, §6). Correctness rests on a
single CPU trace printed at startup (`main.cpp`, `cpuValidation`) and on manual,
ad-hoc image comparisons performed by hand during P2.1. Regressions — a shader
edit perturbing the shadow, a config default drifting, a performance cliff from
a new feature — are caught only when a human looks at a frame.

The physics is **duplicated** between C++ (`metric.cpp`,
`geodesic_integrator.cpp`) and GLSL (`raytrace.comp`), and the **emission
physics** (disk Doppler, jet density/beaming, tone map) exists **only** in GLSL
(CONTEXT §5.8). There is therefore no single implementation a test can treat as
the source of truth, and no automated way to check that the two metric
implementations still agree.

## Solution

Apply the layer structure of [../standards/testing-and-performance.md](../standards/testing-and-performance.md)
to this project:

1. **Extract a GL-free core library** so the pure logic (metric, integrator,
   config) is unit-testable without a GL context — see
   [adr/0005](../adr/0005-extract-blackhole-core.md).
2. **Add a headless automation entry point** that observes the deterministic
   compute output (the post-tone-map `RGBA32F` texture), not the presentation
   surface.
3. **Golden regression** over a fixed set of canned scenes, anchored by two
   cheap invariants that already proved their worth in P2.1: **jet-off
   byte-identity** to the pre-jet baseline, and **disk on/off localized delta**.
4. **Component probes** for the GPU-only emission kernels, via a `physics.glsl`
   split that makes them reachable and removes GLSL duplication.
5. **A performance benchmark** with a committed baseline and a gate.

## User Stories

1. As a contributor, I want unit tests for the metric, integrator and config,
   so that a change to the geodesic math fails fast without a GPU.
2. As a contributor, I want the two metric implementations (C++ and GLSL) to be
   cross-validated, so that they cannot silently drift apart.
3. As a contributor, I want a golden regression suite, so that a shader or
   parameter change that alters the image is caught automatically.
4. As a contributor, I want the **jet-off** render to be byte-identical to the
   pre-jet baseline, so that adding features never perturbs the existing look.
5. As a contributor, I want a **disk on/off** localized-delta check, so that the
   disk's footprint is protected independently of the jet.
6. As a contributor, I want component probes for the GPU-only emission kernels,
   so that the jet density, beaming and tone map are verified against an
   independent CPU reference.
7. As a maintainer, I want a benchmark with a committed baseline and an explicit
   threshold, so that a performance regression is flagged, and a suspicious
   "improvement" is justified against a passing correctness suite.
8. As a maintainer, I want CI to report a missing GPU honestly as `skipped`, so
   that a machine without a capable runtime never produces a green pass.

## Implementation Decisions

### Seams

* **CPU core** — `SchwarzschildMetric`, `GeodesicIntegrator`, `Config`: pure,
  GL-free, unit-testable.
* **Emission kernels** — `jetDensity`, `jetBeamFactor`, `diskEmission` and the
  tone map live only in GLSL; reachable via a probe shader (optionally after an
  extraction into `shaders/physics.glsl`).
* **Observation** — the `RayTracer` post-tone-map `RGBA32F` output texture,
  `NEAREST` filtered: the deterministic seam (CONTEXT §5.6).
* **Automation** — `--render` / `--bench` entry points on the existing
  executable (hidden window; no EGL dependency).

### Core library extraction

`blackhole_core` = core + config + logging; the executable keeps
GL/GLFW/ImGui/window/rendering. Tests link `blackhole_core` only. Full rationale
and the rule that the core stays GL-free: [adr/0005](../adr/0005-extract-blackhole-core.md).

### Reference (golden) format

Store each reference twice: a **raw little-endian `float32`** buffer
(`W·H·4` channels `RGBA`) used for comparison, and an **8-bit `P6` PPM** for
human viewing. The comparator runs on the raw buffers, never on the quantised
PPM. A sidecar records resolution and channel count.

### Scenario matrix

| Scenario | Config | Primary assertion |
|---|---|---|
| jet off | `enabled:false` | **byte-identical** to the pre-jet baseline |
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

### Tolerances

Near-exact for invariants (byte-identity, determinism); looser (`~1e-3` maximum
absolute) where floating-point ordering differs.

### Performance protocol

GPU timer query around the compute dispatch; `--bench` warm-up then measurement;
report median and p95 (never a lone mean); tag results with `GL_RENDERER`. Gate
on `bench/baseline.json` at ±10%. Absolute guideline on the reference GPU
(RTX 4070) at 512×512, 5000 steps: full frame **≤ ~12 ms**, jet **marginal**
cost **≤ ~15%**. Derived diagnostic: average RK4 steps per ray.

### Harness and framework

**CTest** plus a minimal committed header (`tests/check.h`) — no external
dependency, since the vendored `lib/` is gitignored and unversioned.

## Testing Decisions

The harness verifies itself:

* the unit tests must run with **no GL context** and be deterministic and fast;
* the regression comparator is smoke-tested on identical and deliberately
  perturbed buffers;
* a golden reference changes only through the documented update policy (same
  change as the code, with a recorded reason), never as a side effect.

## Out of Scope

* **Full CPU ↔ GPU cross-validation of terminal classification.** The
  mirrored-metric parity check (standards §3.4) is **deferred** within this
  spec: the two implementations' termination thresholds and adaptive-step floors
  intentionally differ (CONTEXT §6), so a first cut relies on the CPU unit tests
  plus golden regression. A follow-up ticket may add the parity check with a
  documented ambiguity band near the photon sphere.
* Provisions for a GPU-capable CI runner beyond **documenting and honouring**
  the skip path; runner provisioning is separate.
* Full radiative-transfer physics, Kerr spin, and any rendering feature — this
  spec is about verification only.
* A test framework dependency; the harness is dependency-free and committed.

## Further Notes

* **No-GPU environments** (for example WSL2 + D3D12 without an accelerator, or a
  display-less runner): the automation entry point must fail loudly or report
  `skipped`, never a nominally green result.
* **Software rendering** (llvmpipe) is numerically different from the GPU; keep a
  separate slow reference set rather than pretending the outputs match.
* Until this lands, CONTEXT §2 ("Tests: None automated") and §6 ("No automated
  tests") remain accurate; update them as each layer is delivered.

---

## Decomposition (tickets)

The rollout is cut into independently deliverable tickets. The first milestone —
**get golden regression running** — is tickets 0008–0011.

| Ticket | Step | Blocked by |
|---|---|---|
| [0008](../ticket/0008-extract-core-library.md) | Extract `blackhole_core` + CTest skeleton | — |
| [0009](../ticket/0009-cpu-unit-tests.md) | CPU unit tests (metric / integrator / config) | 0008 |
| [0010](../ticket/0010-headless-render.md) | Headless `--render` + output readback | 0008 |
| [0011](../ticket/0011-golden-regression.md) | Golden regression harness + first references | 0010 |
| [0012](../ticket/0012-gpu-physics-probes.md) | GPU physics probes + `physics.glsl` split | 0010 |
| [0013](../ticket/0013-benchmark-baseline.md) | `--bench` + committed baseline + performance gate | 0010 |
| [0014](../ticket/0014-ci-wiring.md) | CI wiring with honest skip reporting | 0009, 0011, 0013 |

**Acceptance of this spec:** all seven tickets are Done and the gates are
active — unit tests, golden regression and the performance gate all run and
report honestly.
