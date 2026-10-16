# Changelog — Blackhole Renderer

Milestone and version history. Per the
[documentation lifecycle](./DOCUMENTATION-LIFECYCLE.md), completed tickets are
archived under [`archive/tickets/`](./archive/tickets/) and their execution
summaries under [`archive/summaries/`](./archive/summaries/); this file is the
distilled, durable record.

---

## P2.1 — Relativistic jet + display pipeline (2026-10-16)

Spec: [0001-relativistic-jet](./spec/0001-relativistic-jet.md) · Tickets
[0001](./archive/tickets/0001-foundation-tone-mapping-disk-isco.md),
[0003](./archive/tickets/0003-jet-parameter-plumbing.md)–[0007](./archive/tickets/0007-acceptance-and-polish.md)

- **Display pipeline** — global tone mapping at the end of the compute shader:
  exposure → soft-knee highlight roll-off → optional gamma. Defaults preserve the
  existing look; HDR highlights no longer clip. ([adr/0001](./adr/0001-global-tone-mapping.md)) (`c97d96e`)
- **Disk default fix** — inner radius `3.0 → 6.0` (ISCO, `3·rs`); the misleading
  "ISCO" comment on the old photon-sphere value corrected. ([adr/0002](./adr/0002-disk-inner-radius-at-isco.md)) (`c97d96e`)
- **Jet parameters & UI** — `relativistic_jet` config block, runtime params,
  per-uniform GPU plumbing, shader enable gate, ImGui panel; off by default (`5edec04`).
- **Jet geometry & emission** — bipolar solid cone with a transverse Gaussian,
  bounded axial power-law and soft top fade, integrated over the local proper
  length and added to every terminal colour. ([adr/0003](./adr/0003-emission-only-jet.md)) (`63032e1`, `37ff62a`)
- **Relativistic beaming** — constant bulk velocity `v = sign(z)·β·ẑ`, Doppler
  gain `δ^(3+α)` in the static local frame. ([adr/0004](./adr/0004-jet-constant-bulk-velocity-beaming.md)) (`4185ca9`)
- **Acceptance & polish** — fixed-camera golden images, tuned preset, and a
  radial bounding early-out cutting the jet's frame-time cost to ~12–14%. (`177722f`)

## P1 — Core visual effects (2026-02-26)

- **Gravitational lensing** — procedural starfield, Milky Way band and
  coordinate grid distorted by the curved geometry (`bdce54c`).
- **Photon-sphere shadow** — critical-impact-parameter boundary with photon-ring
  glow from highly-wound orbits (`9d5accd`).
- **Einstein ring** — optional point source behind the hole producing a complete
  ring (`956ab40`, `8cd1a30`).
- **Thin accretion disk** — Novikov–Thorne temperature profile, gravitational
  redshift and Doppler beaming (`a7c64a2`).

## P0 — Core infrastructure (2026-02-25)

- **Project skeleton** — CMake, vendored imgui / nlohmann-json / spdlog,
  logging, config parsing, GLFW window with an OpenGL 4.3 Core context (`d740b27`…`98efa4d`).
- **Schwarzschild metric** — Boyer–Lindquist metric, geodesic RHS and null-ray
  initialisation in C++ (`c001458`).
- **Geodesic integrator** — RK4 on 8 coupled ODEs with adaptive step; CPU
  validation trace at startup (`1d3046c`, `93fe650`).
- **GPU ray marcher** — two-pass compute-shader trace into an `RGBA32F` image,
  presented by a fullscreen quad (`328e8f8`).
