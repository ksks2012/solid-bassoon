# Execution Summary: 0011-golden-regression

- **Date**: 2026-10-20
- **Spec/ADR Path**: `docs/spec/0002-testing-and-performance-harness.md`
- **Ticket Path**: `docs/archive/tickets/0011-golden-regression.md`
- **Branch**: `main`

> Written retroactively on 2026-10-20 during the documentation-lifecycle pass.
> This ticket's session summary was never written (the debt its successor,
> `0012-gpu-physics-probes.md`, flagged), so everything below is reconstructed
> from the commits, the ticket and `tests/reference/README.md`.

---

## 📌 Overview
- **Commits**: `2` commits (`17db1b2`, `bdabe9e`)
- **Files Changed**: `35` files (`+1063` / `-16`)

## 📝 Key Changes
* **Comparator as a committed tool**: `tests/gpu/compare.py` (stdlib only) reports maximum absolute difference, mean absolute difference and the fraction of elements above a threshold over raw `float32` buffers — no image diffing, no dependency. `tests/gpu/test_compare.py` self-verifies it so a broken comparator cannot silently approve a broken frame.
* **Five committed references**: the P2.1 acceptance shots moved from `fig/ticket-0007-*` into `tests/reference/` as raw `.f32` + shape/channel `.meta` sidecars and viewable PPMs, one scene per axis (`jetoff`, `diskoff`, `inclined`, `edgeon`, `nearaxis`). `jetoff` is byte-identical to the pre-jet baseline at `c97d96e` — a contract, not a tolerance.
* **Two invariants**: the golden match (`test_regression_golden`, `tol = 0.0` for `jetoff`, `1e-3` elsewhere ≈ ¼ of an 8-bit code value) and the disk on/off locality check (differences confined to the disk footprint); plus the determinism check that rendering twice yields an identical buffer (`test_headless_render`).
* **Update policy enforced in code, not prose**: `update_reference.py --reason "<why>"` records provenance in the sidecar; `code_hash` refuses a refresh when no code changed (a reference that changes alone is a defect), and a `gl_renderer` mismatch skips honestly rather than rebaselining. `tests/reference/README.md` states the rules (never bump a tolerance to make a new image pass).

## 🧪 Verification
- **Typecheck**: 🟢 Pass — `cmake --build build -j` clean
- **Tests**: 🟢 Pass — **7 tests** at this commit (`test_smoke`, `test_metric`, `test_integrator`, `test_config`, `test_headless_render`, `test_compare`, `test_regression_golden`). Confirmed green again in the two following sessions: `max_abs = 0` on all five scenes, including byte-identical `jetoff`.

## 🔍 Code Review Findings
- **Standards / Spec**: not recorded — the review report from this session did not survive. Inherited contracts were re-checked later: `--rtol`'s `0.0` default (absolute-only golden compares) was explicitly preserved by ticket 0012, and the byte-identity claim was re-confirmed on both D3D12 and llvmpipe.

## 📦 Recent Commits
- `bdabe9e` feat(tests): add golden regression harness and committed float32 references
- `17db1b2` feat(tests): update .gitignore for golden references and add README for reference policy

## ⏭️ Follow-ups
- `--rtol` policy: left at `0.0` because the goldens are same-binary/same-GPU; revisit only if cross-driver references are ever committed.
- `applyToneMap` ceiling: asymptotic-by-construction (`≤ 1.0` asserted by tests, not clamped). An explicit clamp would be an ADR-0001 contract change needing golden re-verification.
- Housekeeping: `/tmp/prejet` worktree at `c97d96e` (referenced by `tests/reference/README.md` rule 10) and `stash@{0}` holding the pre-fix 0011 version — both still undecided.
