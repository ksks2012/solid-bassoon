# Execution Summary: 0005-emission-profile-refinement

- **Date**: 2026-10-15
- **Spec/ADR Path**: `docs/spec/0001-relativistic-jet.md`
- **Ticket Path**: `docs/archive/tickets/0005-emission-profile-refinement.md`
- **Branch**: `main`

---

## 📌 Overview
- **Commits**: 0 (change is uncommitted; fixed point `HEAD` = `63032e1`)
- **Files Changed**: 2 files (`+41` / `-13`)

## 📝 Key Changes
- **`shaders/raytrace.comp` — `jetDensity()` axial profile**: added the bounded axial power-law `axial = pow(1 + |z|/z_falloff, -axial_falloff)`, finite (= 1) on the axis at `z = 0` so there is no central blow-up, then a soft top `axial *= 1 - smoothstep(0.8·max_height, max_height, |z|)` tapering emission to zero before the cut-off.
- **`shaders/raytrace.comp` — shape exponent**: `rho_width = base_radius + |z|^p · tan(opening_angle)`, `p = max(profile_exponent, 0.05)` (clamp avoids `pow(0, …)`). The `profile_exponent` knob is now active: `p = 1` is the cone, lowering it toward `0.5` turns the cone into a parabola via one parameter. A `p == 1` fast path skips the `pow`.
- **`shaders/raytrace.comp` — bounding early-out**: the whole kernel is skipped for rays outside the solid of revolution — axial `|z| >= max_height` and transverse `rho >= 3σ·rho_width`both return `0` before the expensive `exp`/`pow`.
- **`shaders/raytrace.comp` — micro-optimisation**: `pow(rho/rho_width, 2.0)` → `q*q`.
- **`shaders/raytrace.comp` — docs/comments**: the kernel header block now describes the full profile + bounding; the `u_jet_profile_exponent` uniform comment dropped the stale `(reserved)`.
- **`src/main.cpp`**: jet panel note updated to `"Optically-thin cone: transverse Gaussian + axial fall-off; beaming in ticket 0006"`.

## 🧪 Verification
- **Typecheck**: 🟢 Pass — `cmake --build build -j` → `[100%] Built target BlackholeRenderer`; compute shader compiles/links cleanly.
- **Tests**: 🔴 None automated (design; spec forbids a new test seam). **Quantitative manual verification** with a temporary `glReadPixels` read-back on the RTX 4070 (`GALLIUM_DRIVER=d3d12`), disk + point source off, with the final optimised kernel:
  - **Axial profile** (analytic sample at `max_height = 40`): `0:1.0000 5:0.5443 10:0.3536 20:0.1925 30:0.1250 34:0.0914 36:0.0507 38:0.0149 40:0.0000 45:0.0000` — finite at the base, monotonic, smooth taper to zero (acceptance 1–2).
  - **Shape exponent**: `rho_width` at `z = 0` cone(p=1)=**3.20** = parabola(p=0.5)=**3.20**; at `z = 40` cone=**8.11** vs parabola=**3.98** (cone widens with height, parabola stays nearly cylindrical — acceptance 5). ASCII downsample confirms cone (p=1) wider at height vs parabola (p=0.5).
  - **No blow-up**: rendered centre pixel = **0.2985**, frame max = **0.9924**.
  - **Bounding early-out benefit** (acceptance 4), medians of repeated runs, same conditions: early-out **OFF** jetON ≈ 10.19 ms (+42 % over jet-off) vs **ON** jetON ≈ 9.19 ms (+21 %) — i.e. **~10 % faster and roughly halves the overhead**.
  - The temporary verification code was removed before the final build.

## 🔍 Code Review Findings
- **Standards**: No hard violations (CONTEXT §5.1 "mirrored metric implementations" covers geodesic RHS/step/tetrad, not rendering; §4 conventions untouched). Judgement calls / fixes: stale `(reserved)` uniform comment → **fixed**; `Mysterious Name` `zpow` → renamed `axial_shape`; two trivial `if (…) return 0.0;` early-outs left as-is. **Correctness fix**: `smoothstep(edge0 > edge1, …)` is undefined per the GLSL spec → rewritten to the conformant `1.0 - smoothstep(0.8·h, h, z)` (numerically identical to the original; re-checked against the measured profile).
- **Spec**: No blocking/missing items. The "shape exponent" and "capability to turn the cone into a parabola" are **explicitly required** (ticket acceptance 3 & 5) — flagged as scope creep by the review but confirmed in-scope. The transverse `3σ·rho_width` early-out is the ticket's required "cheap bounding test". Emissivity correctly left to the accumulation block (ticket 0004), matching the spec prototype.

## 📦 Recent Commits
- `63032e1` `feat(jet): implement relativistic jet density and update UI message` (ticket 0004 — the fixed point)
- `5edec04` `feat(jet): add relativistic jet configuration and UI controls` (ticket 0003)
- `ff92213` `docs(README): add GPU rendering instructions and update configuration parameters`
- `c97d96e` `feat(render): soft-knee tone mapping, disk ISCO fix, GPU run docs`
