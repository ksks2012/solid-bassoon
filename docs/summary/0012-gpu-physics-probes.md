# Execution Summary: 0012-gpu-physics-probes

- **Date**: 2026-10-20
- **Spec/ADR Path**: `docs/spec/0002-testing-and-performance-harness.md`, `docs/adr/0001-global-tone-mapping.md`, `docs/adr/0005-extract-blackhole-core.md`
- **Ticket Path**: `docs/ticket/0012-gpu-physics-probes.md`
- **Branch**: `main`

---

## 📌 Overview
- **Commits**: 1 (`4264fbf`) — the working tree was 0012-only; 0011 had already landed as `bdabe9e`/`17db1b2`
- **Files Changed**: 21 (`+1938` / `-232`) — 9 new files, 12 modified

## 📝 Key Changes
- **Extraction (`shaders/physics.glsl`, new)** — `diskEmission`, `jetDensity`, `jetBeamFactor`, `applyToneMap`, the `JetShape`/`JetBeam` structs and `PI` moved out of `raytrace.comp` as **pure functions that read no uniforms**; `raytrace.comp` builds the structs in `main()` and passes them at the call sites. `RayTracer::loadShaderSource` gained file-relative, depth-limited (8) `#include` expansion so `#version 430 core` stays the literal first line. `raytrace.comp` went 209 lines down (815 → 606).
- **Probe seam (S2)** — `--probe <file>` in `src/main.cpp`, `runProbe` in `include/app/headless.*` (hidden 64×64 context → `hasComputeContext()` gate → exit `2` when absent) and `RayTracer::runProbe` driving the new `shaders/probe.comp` (input SSBO binding 1, output SSBO binding 2, `local_size_x = 64`).
- **Shared record contract (`include/core/probe_cases.h`, new)** — 48 fixed cases (12 disk / 16 jet density / 10 jet beaming / 10 tone map), input stride 16 float32 (`[0]` kernel id, then slots `a`, `p`, `q`), output stride 4 float32, and **one sidecar writer** (`probe::writeProbeFile`) used by both the CPU and the GPU side so the formats cannot drift.
- **CPU mirrors (S1)** — `core/emission_reference.{h,cpp}` (double precision, GL-free, added to `blackhole_core`) pinned by `tests/unit/emission_reference_test.cpp` with hand-derived literals; `tests/probe_reference.cpp` is the core-only executable that checks table-wide invariants and writes the CPU probe results.
- **Cross-validation + invariant (S3/S4)** — `tests/gpu/probe_check.py` compares CPU↔GPU through `compare.py`, which gained `--rtol` (**default `0.0`**, so the ticket-0011 golden rule is bit-for-bit unchanged); `tests/gpu/no_nan_check.py` renders all five scenes and asserts finite, `≥ 0`, `≤ 1 + 1e-5` (ADR-0001 ceiling), non-blank. Both registered with `SKIP_RETURN_CODE 2`.

## 📊 Verification
- **Typecheck**: 🟢 `cmake --build build -j` clean (no warnings); no separate typecheck step in this C++17 project
- **Test suite**: 🟢 `ctest` **11/11 passed**, ~9.9 s (`GALLIUM_DRIVER=d3d12`)
- **Golden still byte-identical**: 🟢 `regression_check.py` → `max_abs = 0` for `diskoff`/`edgeon`/`inclined`/`jetoff`/`nearaxis`, plus the disk-delta locality invariant — the extraction changed no rendered byte
- **Probe agreement (S3)**: 🟢 48/48 cases; measured `max_abs = 1.07e-4` (on a value of 232) and `max_rel = 2.4e-5`; budget `atol 1e-4 + rtol 1e-5` holds with ≥ 20× margin
- **Full-render invariant (S4)**: 🟢 all five scenes in `[0, 1]`, 0 non-finite, 0 negative, 4–6 % exact zeros (background)
- **Honest skip**: 🟢 without `GALLIUM_DRIVER=d3d12` llvmpipe runs compute, so `test_probe_gpu_cpu` and `test_no_nan` pass there while `test_regression_golden` **skips** on the GL-driver mismatch (reference was made on D3D12)
- **Failure paths exercised**: 🟢 a 0.5 perturbation to a probe record is rejected by `compare.py` (exit 1); `no_nan_check.check_buffer` reports NaN, negative, `> 1.0` and blank buffers as four distinct defects

## 🔍 Code Review Findings
- **Standards**: **0 hard violations.** Core stays GL-free (ADR-0005), the tone map matches ADR-0001, `--rtol`'s `0.0` default preserves the ticket-0011 golden contract, and CONTEXT §5.8 now describes the emission mirrors. Three baseline smells, all judgement calls on the seam layout: *Data Clumps* (`vec4 a/p/q` = the 16-float record), *Repeated Switches* (the 4-way kernel dispatch in `probe.comp`), *Mysterious Name* (terse slot names `a`, `p`, `q`, documented in the record contract).
- **Spec**: 6 findings, no missing acceptance criterion — all five ticket boxes have code **and** a registered CTest test. (a) "golden still passes" is not verifiable from the diff alone. (b) Scope creep: `probe_reference` asserts table-wide finiteness/ceiling invariants beyond "each kernel agrees within tolerance", and `compare.py` grew `--rtol` (both deliberate, documented in the ticket). (c) `applyToneMap` enforces `≤ 1.0` **asymptotically**, not with a clamp, so the ceiling is asserted by tests rather than by the kernel; the jet-beam sign trap (`−n̂`) and the byte-identity claim depend on runtime confirmation.
- **Resolved by verification**: byte-identity confirmed `max_abs = 0` on all five scenes including `jetoff`; probe agreement and the ceiling hold on both D3D12 and llvmpipe.

## 📦 Recent Commits
- `4264fbf` feat(tests): extract pure emission kernels into physics.glsl and add GPU probes
- `bdabe9e` feat(tests): add golden regression harness and committed float32 references

## ⏭️ Follow-ups
- Close the documentation lifecycle for 0008–0012: archive tickets to `docs/archive/tickets/` and add the missing `CHANGELOG.md` entries (debt since 0008).
- `docs/summary/` holds 0008/0009/0010 but **no 0011 summary** — write it alongside that lifecycle pass.
- Decision on the `--rtol` policy for golden compares: left at `0.0` (absolute only) because the goldens are same-binary/same-GPU; revisit only if cross-driver references are ever committed.
- Decide whether `applyToneMap` should gain an explicit `≤ 1.0` clamp (an ADR-0001-contract change requiring golden re-verification) or stay asymptotic-by-construction with tests asserting it.
- Next tickets in spec 0002: **0013** (`--bench` + committed baseline + performance gate) and **0014** (CI wiring with honest skip reporting) — the last unautomated area noted in CONTEXT §6.
- Housekeeping still open: `/tmp/prejet` worktree at `c97d96e` (referenced by `tests/reference/README.md` rule 10) and `stash@{0}` holding the pre-fix 0011 version.
