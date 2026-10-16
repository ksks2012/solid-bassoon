# Execution Summary: 0007-acceptance-and-polish

- **Date**: 2026-10-16
- **Spec/ADR Path**: `docs/spec/0001-relativistic-jet.md`
- **Ticket Path**: `docs/archive/tickets/0007-acceptance-and-polish.md`
- **Branch**: `main`

---

## 📌 Overview
- **Commits**: 0 (change is uncommitted; fixed point `HEAD` = `4185ca9`)
- **Files Changed**: 3 tracked files (`+127` / `-11`) + 4 new binary golden PNGs under `fig/`

## 📝 Key Changes
- **`fig/ticket-0007-{inclined,edgeon,nearaxis,jetoff}.png` — the fixed-camera acceptance set** (512×512, RTX 4070 / D3D12). Captured via a temporary `glReadPixels(GL_BACK)` read-back harness (`--capture`), converted PPM→PNG with a Python stdlib `zlib` encoder. Inclined 65°, edge-on 90°, near-axis 15°, jet-off 80° (jet disabled, disk inner 6.0).
- **`shaders/raytrace.comp` — `jetDensity(pos, rBound)` performance polish.** Added a radial bounding-sphere early-out (`if (r >= rBound) return 0.0;`) before the `cos`, so escaping rays' long outward legs are rejected with one compare. Replaced `ρ = r·sinθ` with `ρ² = r²−z²` (drops the `sin`) and squared the transverse test. `jetRBound = √(max_height² + ρ_max²) + 1` with `ρ_max = 3σ·(base_radius + max_height^p·tan(opening))` is computed once per ray in `main()`. Cuts the jet's frame-time cost from ~18% to **~12–14%**.
- **`README.md` — P2.1 polish.** New "P2.1 — Relativistic Jet" feature table + the four golden images; a "Relativistic Jet" Physics subsection (geometry, additive emission, Doppler beaming, tunable β/α); a "Tuned jet preset (opt-in)" section documenting the shipped values and the recommended 60–75° camera; replaced the stale "not yet rendered (Phase 0 ships the parameter path only)" note.
- **`docs/spec/0001-relativistic-jet.md` — new "Acceptance Record (P2.1)" section** tabulating all 6 acceptance criteria as PASS with evidence (golden image / measured number), including the beaming-asymmetry explanation and the perf method.

## 🧪 Verification
- **Typecheck**: 🟢 Pass — `cmake --build build -j` → `[100%] Built target BlackholeRenderer`; compute shader compiles/links cleanly (harness removed before the final build).
- **Tests**: 🔴 None automated (design; spec forbids a new test seam). **Quantitative manual verification** via the temporary render-output seam:
  - **Golden images** (acceptance 1–4): 4 shots captured and visually inspected; jet-only residuals (`jet on − jet off`) confirm the jet renders correctly at edge-on (symmetric lobes above/below) and near-axis (funnel).
  - **Beaming asymmetry** (acceptance 1): jet-only top/bottom luminance ratio `β=+0.95 → 0.035`, `β=−0.95 → 1.148`, `β=0 → 0.643` — the **sign** is correct (approaching lobe brighter; flip swaps it). Magnitudes are non-reciprocal because the observer is in the upper hemisphere (different near/far geometry per lobe) and the `β=+0.95` lobe hits the tone-map knee (1856 saturated pixels vs 308 for `β=−0.95`, 0 for `β=0`). Expected, not a defect.
  - **Regression** (acceptance 4): built the pre-jet baseline `c97d96e` in a worktree. Jet-off render is **byte-identical** to the baseline at disk inner 6.0 **and** 3.0 (`maxdiff=0`). Pre/post diff is *only* the disk inner edge (15.13% of pixels), exactly reproducing the ISCO fix.
  - **Performance** (acceptance 5): interleaved median of 200 frames with `glFinish()`, jet-on vs jet-off at 512×512 → **~12.4–14.0%** (was ~18% before the early-out). Under the 20% soft target.
  - **Optimization fidelity**: optimized vs committed shader — `inclined` byte-identical, `edgeon` ≤1/255 on 2 channels, `nearaxis` ≤2/255 on 189/786432 channels (0.024%) — floating-point reassociation only.
  - The temporary harness was removed from `src/main.cpp` before the final build.

## 🔍 Code Review Findings
- **Standards**: No hard violations. The `/code-review` Standards sub-agent flagged §5.1 (mirrored metric), §5.6 (per-pixel exactness) and §4 (sign conventions) — **all three verified false positives**: the diff touches only `jetDensity` and one `main()` block, never `geodesicRHS`/`rk4Step`/`initRay`/metric code (the spec states the jet does not touch the mirrored metric contract); §5.6 is about NEAREST texture filtering; §4 signs are unchanged. One genuine judgement note: the optimization is not bit-exact for the **jet-on** path (≤2/255 on ≤0.024% of channels), an expected consequence of the spec-requested early-out; the jet-off path is byte-identical.
- **Spec**: No missing/partial items after verification. The Spec sub-agent's "tuned preset not documented" was a **false positive** (README §"Tuned jet preset (opt-in)" added); its disk-inner-3.0→6.0 and tone-mapping flags concern **already-committed** work, not this diff. Genuine minor: ticket 0007 still reads `Status: blocked (awaiting 0006)` — left untouched, matching the project convention of not editing ticket state during `/implement`. The shader optimization was judged **in scope** (spec requires a bounding early-out; it supports the ≤20% criterion).

## 📦 Recent Commits
- `4185ca9` `feat(jet): implement relativistic Doppler beaming and update UI controls` (ticket 0006 — the fixed point)
- `37ff62a` `feat(jet): refine emission profile with bounded axial power-law and shape exponent adjustments` (ticket 0005)
- `63032e1` `feat(jet): implement relativistic jet density and update UI message` (ticket 0004)
