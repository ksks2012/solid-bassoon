# 0012 — GPU physics probes + `physics.glsl` split

**Spec:** [../spec/0002-testing-and-performance-harness.md](../spec/0002-testing-and-performance-harness.md)

**What to build:** Component probes for the emission kernels that exist only in GLSL — `jetDensity`, `jetBeamFactor`, `diskEmission`, and the tone map — plus the refactor that makes them reachable. Extract the pure kernels into `shaders/physics.glsl`, included by both `raytrace.comp` and a probe compute shader, and compare them against an independent CPU reimplementation.

**Blocked by:** 0010 — needs the headless GL context and readback.

**Status:** Done

- [x] Pure emission kernels are extracted into `shaders/physics.glsl`; `raytrace.comp` includes it and renders identically (golden still passes).
- [x] A probe compute shader feeds fixed inputs through an SSBO and writes kernel outputs for readback.
- [x] Each probed kernel agrees with an independent CPU reimplementation within tolerance (jet profile/falloff/cut; `δ^(3+α)` with `β=0` unbeamed and monotone in `|β|`; disk `r^{-3/4}` shape).
- [x] The tone map is asserted to be **identity below the knee** and `≤ 1.0` above it.
- [x] A no-`NaN`/`Inf`, non-negative invariant runs over a full render.

**How it was done**

- **Extraction.** `shaders/physics.glsl` now owns `PI`, the `JetShape`/`JetBeam` parameter structs and the four pure kernels. `raytrace.comp` gained `#include "physics.glsl"` (expanded textually by `RayTracer::loadShaderSource`, depth-limited to 8) and lost its three kernel definitions plus its own `PI`. `main()` builds `jetShape`/`jetBeam` once and passes them at the call sites, so the kernels read no uniforms. Golden regression: `max_abs = 0` on all five scenes — the split changed no rendered byte.
- **Probe seam (S2).** `--probe <file>` opens a hidden 64×64 context, dispatches `shaders/probe.comp` over the shared record table, reads back the output SSBO and writes it through the same sidecar writer as the CPU side.
- **Shared input table.** `include/core/probe_cases.h` holds 48 fixed cases (12 disk, 16 jet density, 10 jet beaming, 10 tone map) and the record contract: input stride 16 float32 (`[0]` kernel id, `a`, `p`, `q` slots), output stride 4 float32. Both sides `packInputs()` from the same table, so they cannot silently test different inputs.
- **CPU reference (S1).** `include/core/emission_reference.h` + `src/core/emission_reference.cpp` reimplement the kernels in double precision with no GL dependency; `tests/unit/emission_reference_test.cpp` pins hand-derived literals (`0.9` below the knee, `2^3.7 = 12.99603834169977`, `exp(-1/3.2768)`, the disk `r^{-3/4}` ratio, the edge-on Doppler ratio, all three jet cuts `= 0`, …). `tests/probe_reference.cpp` is the core-only executable used by the cross-validation.
- **Cross-validation (S3).** `tests/gpu/probe_check.py` runs `probe_reference --out` and `BlackholeRenderer --probe` and compares them with `compare.py` at `rtol = 1e-5`, `atol = 1e-4` (measured agreement: `max_abs 1.07e-4`, `max_rel 2.4e-5`). `compare.py` gained `--rtol`, defaulting to `0.0`, so the ticket-0011 golden rule is unchanged.
- **Full-render invariant (S4).** `tests/gpu/no_nan_check.py` renders all five scenes and asserts finite, non-negative, `≤ 1 + 1e-5` (ADR-0001 ceiling), and non-blank.
- **Verification.** 11/11 CTest pass with `GALLIUM_DRIVER=d3d12`; the probe and invariant checks also pass on llvmpipe, while the golden honestly skips on the driver mismatch. Failure paths were exercised explicitly (perturbed probe pair rejected; NaN/negative/ceiling/blank buffers each reported).

**See:** [../standards/testing-and-performance.md](../standards/testing-and-performance.md) · [../adr/0005-extract-blackhole-core.md](../adr/0005-extract-blackhole-core.md)
