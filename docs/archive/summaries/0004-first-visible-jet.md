# Execution Summary: 0004-first-visible-jet

- **Date**: 2026-10-15
- **Spec/ADR Path**: `docs/spec/0001-relativistic-jet.md`
- **Ticket Path**: `docs/archive/tickets/0004-first-visible-jet.md`
- **Branch**: `main`

---

## 📌 Overview
- **Commits**: 0 (change is uncommitted; fixed point `HEAD` = `5edec04`)
- **Files Changed**: 2 files (`+57` / `-8`)

## 📝 Key Changes
- **`shaders/raytrace.comp` — jet density kernel**: new `jetDensity(vec4 pos)` implementing the axisymmetric solid cone (`z = r·cosθ`, `ρ = r·sinθ`) with `rho_width = base_radius + |z|·tan(opening_angle)` and a transverse Gaussian `exp(-(ρ/rho_width)² / (2σ²))`, bounded by a hard axial extent `|z| < max_height`. Introduced `JET_COLOR` (cool blue-white).
- **`shaders/raytrace.comp` — accumulation**: filled the ticket-0003 insertion point with a `u_jet_enabled`-gated per-step block that samples the kernel once and integrates over the local proper length `ds = h_eff / sqrt(f)`, `f = max(1 − rs/r, 1e-4)`. It sits **before** the disk-crossing test so a disk-terminating ray carries exactly the foreground jet up to the hit.
- **`shaders/raytrace.comp` — compositing**: `color += jetAccum;` added after all terminal branches (captured / disk / escaped / max-steps) and before the tone map — optically thin and additive, never occluding the disk, stars or Einstein rings.
- **`src/main.cpp`**: updated the jet panel's stale "Jet geometry not rendered yet" note to describe the shipped cone + transverse Gaussian.

## 🧪 Verification
- **Typecheck**: 🟢 Pass — `cmake --build build -j` → `[100%] Built target BlackholeRenderer`; compute shader compiles/links cleanly.
- **Tests**: 🔴 None automated (design; spec forbids a new test seam). **Quantitative manual verification** via a temporary `glReadPixels` read-back on the RTX 4070 (`GALLIUM_DRIVER=d3d12`):
  - Visibility: enabling the jet raises mean luminance **0.0907 → 0.2985** (~3.3×); 97 532 / 262 144 pixels changed.
  - Step-independence: mean luminance at `step×2` = 0.29873 vs `step×0.5` = 0.29844 (**~0.1 %** apart).
  - Isolation: with disk + point source off, jet off 0.0569 → jet on 0.2820, confirming the effect is the jet.
  - Shape: an ASCII downsample of the isolated jet shows a clear **bipolar structure along the vertical (polar) axis** with a waist at the equatorial plane (default inclination 80°).
  - The temporary verification code was removed before final build.

## 🔍 Code Review Findings
- **Standards**: No hard violations (state/pack conventions and the "physics mirrored in C++ + GLSL" contract are respected — the jet is rendering-only, like `diskEmission`). Judgement calls: redshift-floor duplication with a differing ε (`1e-4` vs disk's `0.01`) — left as-is (unifying would touch disk visuals; the floor is unreachable before capture); re-reading `pos.y` — **fixed** to reuse `r`. One fixed.
- **Spec**: No blocking/missing items. Deferred-by-design (owned by 0005/0006, not defects): `profile_exponent` unused, radial `3σ·ρ_width` early-out absent, smooth axial fall-off/soft top absent. Scope-creep note: hardcoded `JET_COLOR` is not specified in the spec (accepted, documented).

## 📦 Recent Commits
- `5edec04` `feat(jet): add relativistic jet configuration and UI controls` (ticket 0003 — provides this ticket's parameters)
- `ff92213` `docs(README): add GPU rendering instructions and update configuration parameters`
- `c97d96e` `feat(render): soft-knee tone mapping, disk ISCO fix, GPU run docs`
- `234ef36` `docs(tickets): add foundational documents for tone mapping, jet parameters, and emission profiles`
