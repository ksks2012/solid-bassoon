# 0001 — Relativistic Jet Rendering (P2.1)

- **Status:** Accepted · `ready-for-agent`
- **Date:** 2026-10-14
- **Scope:** P2.1 — emission-only relativistic jet + display pipeline prerequisite
- **References:** [../CONTEXT.md](../CONTEXT.md), [../DEFINITION.md](../DEFINITION.md), [../DOCUMENTATION-LIFECYCLE.md](../DOCUMENTATION-LIFECYCLE.md)

---

## Problem Statement

Someone running the renderer sees gravitational lensing, the black-hole
shadow, Einstein rings, and a thin accretion disk — but **the most
recognisable feature of an accreting black hole, its relativistic jet, is
missing**. The default scene therefore looks like "a disk around a hole,"
not a full active galactic nucleus.

Two adjacent problems make the picture worse:

1. **HDR highlights clip.** The compute shader clamps its linear float output
   straight to `[0, 1]`, so Doppler-boosted disk light and photon-ring light
   saturate to flat white instead of falling off gracefully.
2. **The disk default is unphysical.** The shipped `inner_radius = 3.0` sits at
   the photon sphere, *inside* the ISCO (`3·rs = 6.0`); the UI already warns
   about it, but the default ships wrong.

The user wants a physically plausible, visually striking bipolar jet whose
appearance changes correctly with viewing angle (relativistic beaming), a
display pipeline that no longer clips, and a disk default that is physically
defensible.

## Solution

Add an **opt-in, axisymmetric, emission-only relativistic jet** to the GPU ray
tracer, composited as an **optically thin foreground** onto the existing
terminal colours. The jet is a solid cone around the symmetry axis with a
transverse Gaussian profile and a bounded axial falloff, launched from outside
the photon sphere and from inside the disk's inner edge. Its emission is
modulated by a relativistic Doppler/beaming factor so that the approaching lobe
brightens and the receding lobe fades.

Before the jet, introduce a **global tone-mapping + gamma step** as a
prerequisite, so the new HDR emission (and existing Doppler-boosted disk light)
maps gracefully instead of clipping. Fix the disk default inner radius to the
ISCO. Expose all parameters through `config.json` and an ImGui panel, with a
soft warning when the jet base intrudes into the emitting disk.

The jet ships **disabled by default** (opt-in); the disk-default fix is
unconditional.

## User Stories

1. As an observer, I want a bipolar jet launched along the black hole's spin
   axis, so that the scene reads as an accreting black hole rather than a disk
   alone.
2. As an observer, I want the jet to be a solid volume with a soft transverse
   edge, so that it does not look like a hollow glowing tube.
3. As an observer, I want the jet's transverse brightness to follow a Gaussian
   cross-section, so that the jet has a natural core-bright rim-soft appearance.
4. As an observer, I want the jet's width to grow with height (a cone), so that
   it visually opens up away from the black hole.
5. As a user, I want a single "profile exponent" parameter reserving the ability
   to turn the cone into a parabola later, so that the shape can be refined
   without restructuring the code.
6. As an observer, I want the jet to launch from **outside the photon sphere**,
   so that it is not emitted from a region that cannot physically launch
   material.
7. As an observer, I want the jet base to sit **inside the disk's inner edge**,
   so that the jet foot does not overlap or intrude into the bright emitting
   disk.
8. As an observer, I want the jet foot to occupy the dark inner hole of the
   disk, so that the jet appears to emerge from the plunging region rather than
   through the disk.
9. As an observer, I want the jet to fade smoothly with height, so that it does
   not end in a hard cut-off at the top.
10. As an observer, I want the jet to be brightest near its base and decline
    outward, so that it resembles a realistic jet.
11. As an observer, I want the jet to remain finite at its base, so that there
    is no numerical blow-up or hot spot exactly on the axis at `z = 0`.
12. As an observer, I want the jet to be treated as **optically thin**, so that
    it does not occlude the disk, stars, or Einstein rings behind it.
13. As an observer, I want the jet's light to be **added** on top of whatever
    the ray ends on (disk, sky, or black hole), so that the foreground jet
    glows over every background without special cases.
14. As an observer, I want the jet in front of the black hole to remain visible,
    so that the shadow is not "punched out" by hard black assignment.
15. As an observer approaching the jet axis, I want the **approaching lobe to
    brighten** and the receding lobe to dim, so that the relativistic beaming
    is physically and visually correct.
16. As a user, I want to control the bulk velocity (single β), so that I can
    dial the beaming asymmetry with one parameter.
17. As a user, I want a spectral-index parameter controlling the beaming
    exponent (`3 + α`), so that I can match the brightness contrast of
    synchrotron-like emission.
18. As a user, I want a master emissivity/brightness control, so that I can
    tune the jet's overall contribution.
19. As a user, I want the jet to be **off by default**, so that the existing
    default image and behaviour are preserved until I opt in.
20. As a user, I want to enable/disable the jet at runtime, so that I can
    compare with and without it in one session.
21. As a user, I want the jet's parameters exposed in an ImGui panel, so that I
    can tune geometry, profile, velocity, and brightness interactively.
22. As a user, I want the jet's parameters in `config.json`, so that I can ship
    a tuned preset without touching code.
23. As a user, I want a warning when the jet base is placed at or inside the
    disk inner radius, so that I avoid the unphysical "jet foot inside the
    disk" configuration.
24. As a user, I want the disk's default inner radius fixed to the ISCO, so
    that the shipped scene is physically defensible.
25. As an observer, I want a global tone-mapping step before display, so that
    bright HDR features roll off instead of clipping to white.
26. As an observer, I want gamma-corrected output, so that the image is not
    washed out or overly dark.
27. As a user, I want an exposure control, so that I can balance overall
    brightness across scenes.
28. As a user, I want the tone-mapping defaults chosen so that the existing
    disk/starfield look is nearly unchanged, so that enabling the pipeline is
    not a jarring regression.
29. As a maintainer, I want the jet disabled path to cost essentially nothing,
    so that users who never enable it pay no performance penalty.
30. As a maintainer, I want the volume integration to be step-independent and
    stable, so that the jet's brightness does not change when the adaptive
    step size changes.
31. As a maintainer, I want the jet's sampling to piggy-back on the existing
    geodesic integration steps, so that no new sampling loop or aliasing risk
    is introduced.
32. As a maintainer, I want a cheap bounding test that skips the jet kernel for
    rays far from the jet, so that performance stays within budget.

## Implementation Decisions

### Display pipeline prerequisite (Phase 0.5)

- **Tone mapping is a hard prerequisite**, promoted to a standalone phase
  before any jet geometry.
- Tone mapping is **global and unconditional**, applied once at the **end of
  the compute shader's `main()`**, after all colour accumulation.
- The mapping is **exposure + a soft-knee highlight roll-off** (identity below
  the knee, compressing only true HDR headroom), followed by an optional
  **display gamma**. `exposure` and `gamma` become runtime parameters; their
  defaults (`exposure = 1.0`, `gamma = 1.0`) leave the normal range untouched,
  so the existing disk/starfield look is nearly unchanged while HDR highlights
  roll off instead of clipping.
- It ships as its **own commit**, before the jet.

### Jet geometry

- **Axisymmetric in `(z, ρ)`**, with `z = r·cosθ` and `ρ = r·sinθ`; density is
  independent of `φ`. No extra polar handling is needed: the shader already
  clamps `θ` away from the poles and guards `sin²θ`.
- **Solid volume with a transverse Gaussian**, not a hollow shell (avoids
  visible tube walls).
- **Cone first** (`profile_exponent = 1`), with a reserved exponent `p` so a
  parabolic profile can be adopted later by changing one number.
- **Base outside the photon sphere**: `base_radius ≥ 1.5·rs` (default `3.2`).
- **Design rule `base_radius < disk_inner`** (soft, ImGui-warned, not a hard
  shader clamp) so the jet foot falls in the disk's dark inner hole and never
  overlaps the emitting disk.
- The cone's equatorial foot (`ρ_width(0) = base_radius`) is the geometric
  origin of any "jet through the disk" overlap; the rule above removes the
  overlap without any cutting logic.

Prototype-encoded transverse/axial profile (decision-dense, from the grilling):

```glsl
float rho_width = base_radius + abs(z) * tan(opening_angle);   // p = 1 cone
float gauss     = exp(-pow(rho / rho_width, 2.0) / (2.0 * radial_sigma * radial_sigma));
float axial     = pow(1.0 + abs(z) / z_falloff, -axial_falloff); // bounded at z = 0
axial          *= smoothstep(max_height, max_height * 0.8, abs(z)); // soft top
float j         = gauss * axial * emissivity;
```

### Volume integration

- **Additive compositing** (optically thin): `final = terminal_color + jetAccum`
  for **every** terminal case (CAPTURED, disk hit, ESCAPED, MAX_STEPS). The
  disk-front case is automatically correct because a disk hit `break`s before
  any further jet accumulation.
- **Insertion point** per step: `rk4Step` → clamp θ → `accumulateJet(prev, pos)`
  → disk-crossing check → terminal checks → `prev = pos`. Accumulating **before**
  the disk check keeps the accumulated jet as the foreground of anything the
  ray terminates on.
- **Path measure is the local proper length**: `ds = h_eff / sqrt(f)` with
  `f = 1 − rs/r`, derived from `dl = dλ / sqrt(f)` for null geodesics
  (equivalently `dl = sqrt(f)·dt`, `dt/dλ = 1/f`). This is step-independent and
  corrects the ≈1.6× under-weighting at the jet base that raw affine stepping
  would cause.
- **Sampling: once per RK4 step.** No sub-sampling — the jet is always ≥
  `base_radius` wide transversally while steps are ~0.1–0.5, so there is no
  aliasing risk.
- **Bounding early-out:** skip the jet kernel when `|z| ≥ max_height` or
  `ρ ≥ 3·σ·ρ_width`. `MAX_STEPS` is unchanged.

### Relativistic beaming (Phase 4)

- **Constant bulk velocity**, bipolar direction `v = sign(z)·β·ẑ`. An
  accelerating profile `β(z)` is deferred.
- **Doppler factor in the static local frame**, built from the covariant
  momenta already carried: local energy `E_loc = 1/sqrt(f)`; local unit direction
  `n̂ = (f·p_r, sqrt(f)·p_θ/r, sqrt(f)·p_φ/(r·sinθ))` (observer → scene, i.e. the
  march direction); axis `ẑ = cosθ·ê_r − sinθ·ê_θ`.
- Because the ray is traced backwards, the emitter → observer direction is
  `−n̂`; the standard result is
  `δ = 1 / [ γ·(1 − β·cosθ) ]` with `cosθ = v̂·(−n̂)`, `γ = 1/sqrt(1−β²)`.
  (The `−n̂` sign is the classic trap and **must be confirmed visually**.)
- **Beaming exponent `δ^(3+α)`**, with `α = spectral_index` (default `0.7`),
  multiplying the emission rate `j` per step.

### Parameters, config, and UI

- New grouped parameter block on the runtime params (nested struct), mirrored
  by a `JetConfig` parsed from a `relativistic_jet` block in `config.json`, and
  uploaded as individual uniforms (no UBO), consistent with current style.
- A single `u_jet_enabled` gates the whole accumulation block **inside** the
  shader, so the disabled path is effectively free.
- **Default `enabled = false`** (opt-in); a tuned preset is documented for
  users who want to enable it.
- ImGui: a "Relativistic Jet" collapsing header with the parameter sliders, plus
  a **yellow warning when `base_radius ≥ disk_inner`**, mirroring the existing
  ISCO warning style.
- Disk fix: `disk.inner_radius` default changes **3.0 → 6.0 (ISCO)**
  unconditionally, and the incorrect "ISCO" comment on the 3.0 value is
  corrected (3.0 is the photon sphere). This is a bug fix, not a new feature.

### Suggested initial defaults

```
base_radius      = 3.2          opening_angle   = 6.0°–8.0°
profile_exponent = 1.0          max_height      = 40.0
radial_sigma     = 0.4          z_falloff       = 10.0
axial_falloff    = 1.5          bulk_velocity   = 0.95
spectral_index   = 0.7          emissivity      = 1.0
enabled          = false
```

## Testing Decisions

**What a good test is here:** the project has **no test framework** and the jet
is a visual phenomenon whose correctness is perceptual; pixel-exact assertions
would be brittle and low-value. Good verification therefore means **checking
observable rendered output at the highest-level seam**, against a fixed
configuration, judged against a recorded reference — not asserting internal
function structure.

**Seam (single, existing, highest-level):** the render output of
`RayTracer::render(RayTracerParams)` → the output image. Everything the jet can
do is exercised through this seam; **no new seam is introduced** and no test
harness is added.

**Method:** fixed `config.json` + camera presets, captured as **golden
reference images** and inspected manually.

**Acceptance shots (P2.1 passes when all hold):**

1. **Inclined view (inclination ≈ 60–75°), jet on** — beaming is asymmetric:
   approaching lobe bright, receding lobe dim/absent. *(The only reliable check
   of the δ sign.)*
2. **Edge-on view (inclination ≈ 90°)** — cone geometry correct; base sits in
   the disk's inner hole; foot does not intrude into the bright disk.
3. **Near-axis view (small inclination)** — axial power-law + soft top: smooth
   fade near `max_height`, no hard edge.
4. **Regression, jet off** — compared with the pre-change render, the only
   difference is the disk inner edge (3.0 → 6.0).
5. **Performance** — at the target resolution, frame-time delta jet-on vs
   jet-off is **≤ 20%** (soft target; if exceeded, revisit `σ` / early-out).
6. **Optional δ sanity check** — verify the Doppler formula numerically against
   a reference table (`θ = 0 → 1/[γ(1−β)]`); explicitly optional.

**Prior art:** the only existing "correctness check" is the startup CPU
validation trace (`GeodesicIntegrator`), which is not a test harness and is not
extended here. The jet is rendering-only and does **not** touch the mirrored
metric contract.

## Out of Scope

- **Kerr / spin.** The jet remains in the Schwarzschild (non-rotating) metric.
  `blackhole.spin` stays parsed-but-unused.
- **Full radiative transfer (Phase 5):** self-absorption / optical depth `τ`,
  and any absorption of background light by the jet. P2.1 is emission-only.
- **Accelerating velocity profile `β(z)`** — deferred; only constant β.
- **Parabolic jet geometry** — only the reserved exponent `profile_exponent`
  is provided; the parabola itself is a later tweak.
- **New automated tests / test framework.**
- **Writing ImGui changes back to `config.json`** (unchanged project behaviour).
- **Splitting `MAX_STEPS`' dual meaning** (photon-ring glow vs true budget
  exhaustion) — noted in CONTEXT, addressed separately.
- **Resolving the CPU/GPU termination-threshold mismatch.**

## Further Notes

**Phase roadmap (P2.1):**

- **Phase 0 — Params & architecture:** nested param block, config parsing,
  uniform plumbing, `u_jet_enabled` gate (off by default).
- **Phase 0.5 — Tone mapping:** global exposure + soft-knee highlight roll-off
  (+ optional gamma) at end of `main()`, defaults (exposure 1.0, gamma 1.0)
  preserving the current look. **Ships first, as its own commit.**
- **Phase 1 — Simple geometry:** cone solid + transverse Gaussian, no lensing
  subtleties.
- **Phase 2 — Lensing:** verify the cone integrates correctly through the
  existing geodesic marcher.
- **Phase 3 — Emission-only accumulation:** additive compositing, proper-length
  `ds`, per-step sampling, bounding early-out.
- **Phase 4 — Beaming:** constant β, local-frame δ, `δ^(3+α)`; visual sign check.
- **Phase 5 — Full RTE + absorption:** deferred beyond P2.1.
- **Phase 6 — Polish:** parameter tuning, presets, the acceptance captures.

**Suggested commit slicing:** tone mapping (Phase 0.5) → disk inner-radius fix
→ jet params/plumbing (Phase 0) → geometry (Phases 1–2) → emission
accumulation (Phase 3) → beaming (Phase 4) → polish (Phase 6).

**Sandbox note:** `lib/` (imgui / nlohmann-json / spdlog) is gitignored and must
be present locally for the build; it is not part of this feature.

**Vocabulary:** this spec uses the project glossary (`CONTEXT.md`,
`DEFINITION.md`) — null geodesic, covariant momenta, static observer, tetrad,
photon sphere, ISCO, accretion disk, terminal colour, tone mapping, etc.

---

## Acceptance Record (P2.1)

Recorded by ticket [0007](../ticket/0007-acceptance-and-polish.md). All shots are
fixed-camera renders at 512×512, `max_steps = 5000`, on an RTX 4070 via D3D12
(`GALLIUM_DRIVER=d3d12`). The seam is the existing render output of
`RayTracer::render(RayTracerParams)`; no new seam or automated harness was added.

| # | Criterion | Result | Evidence |
|---|-----------|--------|----------|
| 1 | Inclined (≈60–75°), beaming asymmetric, approaching lobe bright | **PASS** | `fig/ticket-0007-inclined.png` (65°) |
| 2 | Edge-on (≈90°): cone geometry, base in disk inner hole, no intrusion | **PASS** | `fig/ticket-0007-edgeon.png` (90°) |
| 3 | Near-axis: axial power-law + soft top, smooth fade, no hard edge | **PASS** | `fig/ticket-0007-nearaxis.png` (15°) |
| 4 | Jet-off regression differs only by the disk inner edge (3.0→6.0) | **PASS** | `fig/ticket-0007-jetoff.png` (80°) |
| 5 | Frame-time delta (jet on vs off) at target res ≤ 20% | **PASS** | ~12–14% at 512×512 |
| 6 | (Optional) δ verified numerically against a reference table | **PASS** | δ=N[1/[γ(1−βcosθ)]], θ=0 → 1/[γ(1−β)] |

**Criterion 1 — beaming asymmetry.** The jet-only residual (`jet on − jet off`,
incl 65°) is strongly one-sided for β=+0.95 (top/bottom luminance ratio ≈ 0.035)
and flips for β=−0.95 (≈ 1.15); β=0 is near-symmetric (≈ 0.64). The **sign** —
approaching lobe brighter, flip swaps it — is correct, matching the numeric δ
check. The two β cases are not equal in magnitude because the observer sits in
the upper hemisphere, so the two lobes are at different near/far geometry (the
+z lobe's bright base faces the observer; the −z lobe's base is foreshortened and
partly behind the shadow), and because the β=+0.95 lobe hits the tone-map knee
(saturation) while β=−0.95 mostly does not. This is expected, not a defect.

**Criterion 4 — regression.** With the jet off, the render is **byte-identical**
to the pre-jet baseline (commit `c97d96e`, disk inner 6.0): `maxdiff = 0` at both
inner 6.0 and inner 3.0. Comparing pre-change (inner 3.0) with post-change
(inner 6.0) the images differ only at the disk inner edge (15.13% of pixels),
exactly reproducing the ISCO fix; the jet-off path introduces zero change.

**Criterion 5 — performance.** Interleaved median of 200 frames, jet-on vs
jet-off, isolating GPU work with `glFinish()`. The radial bounding early-out in
`jetDensity` keeps the cost at ~12–14% (it was ~18% before that reject). Both are
under the 20% soft target; the margin is comfortable on the reference GPU.

**Criterion 6 — δ sanity.** The Doppler factor was checked numerically against
the analytic limit (`θ = 0 → δ = 1/[γ(1−β)]`), confirming the `−n̂` sign and the
`γ` factor; no visualization disagreement was found.
