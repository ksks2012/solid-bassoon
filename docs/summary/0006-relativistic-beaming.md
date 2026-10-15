# Execution Summary: 0006-relativistic-beaming

- **Date**: 2026-10-15
- **Spec/ADR Path**: `docs/spec/0001-relativistic-jet.md`
- **Ticket Path**: `docs/ticket/0006-relativistic-beaming.md`
- **Branch**: `main`

---

## 📌 Overview
- **Commits**: 0 (change is uncommitted; fixed point `HEAD` = `37ff62a`)
- **Files Changed**: 4 files (`+52` / `-13`)

## 📝 Key Changes
- **`shaders/raytrace.comp` — new `jetBeamFactor(pos, mom, f)`**: builds the Doppler factor in the static local frame from the covariant momenta the ray already carries (`mom = (p_t, p_r, p_θ, p_φ)`, normalised to `E = -p_t = 1`). Local march direction `n̂ = (f·p_r, √f·p_θ/r, √f·p_φ/(r·sinθ))`; jet axis `ẑ = cosθ·ê_r − sinθ·ê_θ`; bipolar bulk direction `v̂ = sign(z)·ẑ`; `δ = 1/[γ(1 − β·(v̂·(−n̂)))]` with `γ = 1/√(1−β²)`; returns `δ^(3+α)` with `α = spectral_index`. `β = 0` early-outs to `1` (unbeamed).
- **`shaders/raytrace.comp` — accumulation**: `jetAccum += JET_COLOR · (emissivity · j · jetBeamFactor(pos, mom, f)) · ds` — beaming multiplies the emission rate `j` once, multiplicatively, with no double counting.
- **`src/main.cpp`**: widened the "Bulk Velocity" slider to `[-0.999, 0.999]` so a negative β (which swaps which lobe is approaching) is reachable from the UI; jet panel note updated.
- **Stale-comment cleanup**: dropped "(reserved)" from `profile_exponent` in `include/app/config.h` / `include/rendering/ray_tracer.h`, and fixed the "draws nothing until the geometry kernel lands" line in `JetParams`; trimmed the stale "Phase 0 / Ticket 0004 fills it in" line in the shader accumulation comment.

## 🧪 Verification
- **Typecheck**: 🟢 Pass — `cmake --build build -j` → `[100%] Built target BlackholeRenderer`; compute shader compiles/links cleanly.
- **Tests**: 🔴 None automated (design; spec forbids a new test seam). **Quantitative manual verification** with a temporary `glReadPixels` read-back on the RTX 4070 (`GALLIUM_DRIVER=d3d12`), disk/source off, inclination 60°, using a **jet-only residual** (jet-on minus jet-off background frame) so the starfield is subtracted out:
  - **Sign swap** (acceptance 1–2): top/bottom-half jet luminance ratio — `β=0 → 0.926`, `β=+0.95 → 0.416`, `β=−0.95 → 2.000`. Flipping β inverts which lobe is bright; β=0 is symmetric. ASCII downsamples confirm a bright vs. dark lobe per case.
  - **Correct lobe** (not just self-consistent): the observer sits at `θ_obs=60°` and the local `ê_θ` (screen-up) = `(cos60,0,−sin60) = (0.5,0,−0.866)`, so **screen-up points toward −z**; the +z lobe moving `+ẑ` is the one approaching the observer and it is the brighter one → **approaching lobe brighter** ✓.
  - **Unbeamed baseline** (acceptance 3): `β=0` → ratio 0.93 (near-symmetric; residual tilt is lensing/geometry, not beaming).
  - **Multiplicative** (acceptance 4): `jetDensity` returns pure `gauss·axial`; beaming enters once via `j * jetBeamFactor(...)`, `ds` separate.
  - **δ sanity** (optional acceptance 6): `β=0.95, cosθ=1 → δ^(3.7) = 877.95`, exactly `1/[γ(1−β)]^3.7` (verified in Python).
  - **Perf**: beaming adds one `pow` per jet sample. Jet-on ~9.33–9.39 ms vs jet-off ~7.5 ms (≈+23–25%, median-of-repeats with occasional WSL/D3D12 noise), i.e. within noise of the pre-beaming ~+21% from ticket 0005 — negligible added cost.
  - The temporary verification code was removed before the final build.

## 🔍 Code Review Findings
- **Standards**: No hard violations (§5.1 "mirror in C++ + GLSL" is scoped to geodesic RHS/step/tetrad, which are untouched; emission physics is GLSL-only like the disk Doppler). Judgement calls, fixed: the new comment asserted `E_loc = 1/√f`, which **contradicted** the file's existing tetrad comment (`E_loc = √f`) — reworded to avoid the conflicting claim and describe the carried momenta instead; `jetBeaming` named a process but returned a scalar gain → **renamed** `jetBeamFactor`; duplicated `f` recomputation → **passed `f` in** as a parameter. One pre-existing inconsistency found and documented but left alone (out of scope): the GLSL camera sets `p_t = -1` but `p_r = n_r`, which does **not** satisfy the null condition except at infinity, so the code's momentum convention is `E_loc = √f`; under it the spec's `n̂` formula evaluates to `f·`(unit direction), which slightly weakens the beaming near the bright base. Ticket 0006 implements the spec's formula faithfully.
- **Spec**: No missing/partial items; all 4 acceptance boxes met. Slider widen to negative β is *required* by acceptance 2, not creep. Comment-only scope creep (removing "(reserved)", rewriting the JetParams header) accepted as harmless doc fixes. `abs(β)<1e-5` early-out flagged as redundant (δ=1 already at β=0) but kept as a cheap guard + `pow` skip. Added a `clamp(cosAng, -1, 1)` after both reviewers raised a possible NaN if `|n̂|` drift made `1−β·cosAng ≤ 0` (no-op given `|cosAng| ≤ f < 1`, but defensive).

## 📦 Recent Commits
- `37ff62a` `feat(jet): refine emission profile with bounded axial power-law and shape exponent adjustments` (ticket 0005 — the fixed point)
- `63032e1` `feat(jet): implement relativistic jet density and update UI message` (ticket 0004)
- `5edec04` `feat(jet): add relativistic jet configuration and UI controls` (ticket 0003)
