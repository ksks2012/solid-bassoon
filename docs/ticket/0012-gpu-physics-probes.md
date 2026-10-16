# 0012 — GPU physics probes + `physics.glsl` split

**Spec:** [../spec/0002-testing-and-performance-harness.md](../spec/0002-testing-and-performance-harness.md)

**What to build:** Component probes for the emission kernels that exist only in GLSL — `jetDensity`, `jetBeamFactor`, `diskEmission`, and the tone map — plus the refactor that makes them reachable. Extract the pure kernels into `shaders/physics.glsl`, included by both `raytrace.comp` and a probe compute shader, and compare them against an independent CPU reimplementation.

**Blocked by:** 0010 — needs the headless GL context and readback.

**Status:** Todo

- [ ] Pure emission kernels are extracted into `shaders/physics.glsl`; `raytrace.comp` includes it and renders identically (golden still passes).
- [ ] A probe compute shader feeds fixed inputs through an SSBO and writes kernel outputs for readback.
- [ ] Each probed kernel agrees with an independent CPU reimplementation within tolerance (jet profile/falloff/cut; `δ^(3+α)` with `β=0` unbeamed and monotone in `|β|`; disk `r^{-3/4}` shape).
- [ ] The tone map is asserted to be **identity below the knee** and `≤ 1.0` above it.
- [ ] A no-`NaN`/`Inf`, non-negative invariant runs over a full render.

**See:** [../standards/testing-and-performance.md](../standards/testing-and-performance.md) · [../adr/0005-extract-blackhole-core.md](../adr/0005-extract-blackhole-core.md)
