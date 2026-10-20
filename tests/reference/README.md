# Golden references — policy (ticket 0011)

The golden suite (`tests/reference/*.f32`) captures the exact GPU output of a
single, pinned renderer/config on the reference machine (WSL2+D3D12, driver
as recorded) for each scene:
- `jetoff.json` — jet off, disk on (inclination 80°) — regression baseline
- `diskoff.json` — jet off, disk off (inclination 80°) — disk toggle
- `inclined.json` (65°, jet on, disk on)
- `edgeon.json` (90°, jet on, disk on)
- `nearaxis.json` (15°, jet on, disk on)

Rules:
1. These are **hardware/driver-config artifacts**. Regenerating on a different
   GPU/driver is only allowed if it is a deliberate, recorded upgrade.
2. Never “bump” tolerances to make a new image pass. If the new image is
   correct by physics/spec, regenerate references with `--reason` and explain.
3. **Always record provenance.** `update_reference.py --reason "<why>"` writes
   scene, source, reason and date to the `.meta` sidecar. The reason must be
   specific (e.g. "shader fix: diskEmission +theta").
4. Jet-off is exact by contract: the regression runner uses `tol = 0.0` for
   `jetoff`, because spec/0001 requires byte identity with the pre-jet baseline.
   The remaining scenes use `1e-3` (≈1/4 of one 8-bit code value) — see
   `tests/gpu/reference.py` for the justification.
5. Disk on/off difference must be localised to the committed disk footprint
   (not the black border, not the full frame) — see regression_check.py.
6. Do **not** hand-edit `.f32/.ppm`. Only the generator + renderer may write
   golden buffers.
7. References are binary (float32 RGBA). Include sidecars. PPMs are provided
   for human viewing only (never compared); they are a gamma-2.2 encoded copy
   of the raw buffer, recorded as `view_gamma` in the sidecar.
8. A reference change ships in the **same commit** as the code change that
   caused it, with `--reason` recorded — never in a commit of its own
   (`standards/testing-and-performance.md` §3). Conversely, a reference that
   changes while the rendering code did not is a **defect**, not a refresh.
   Enforced: each sidecar records `code_hash` (sha256 over `shaders/*`,
   `src/rendering/*`, `include/rendering/*`, `src/app/{config,render_params}.*`
   and the scene's own config). `update_reference.py` **refuses** to replace a
   reference when `code_hash` is unchanged but the bytes differ; only an
   explicit `--force` overrides that, and `--reason` is always recorded.
9. **Driver identity.** Each sidecar records `gl_renderer=` (the GL_RENDERER
   string the reference was generated on). `regression_check.py` compares it
   with the live driver and **skips** (exit 2) on a mismatch — llvmpipe and the
   D3D12 GPU are numerically different (`spec/0002` "Further Notes"), so a
   mismatch is never a pass and never a confusing numeric failure. To move
   references to another driver, regenerate with `--reason` in the same commit.
10. **Provenance:** `jetoff.f32` was verified byte-identical (`max_abs = 0`)
   against the pre-jet baseline at commit `c97d96e` (disk inner 6.0), as
   required by `spec/0001` §"Acceptance". Re-verify with that worktree whenever
   the jet is touched.
