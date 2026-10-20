# Execution Summary: 0010-headless-render

- **Date**: 2026-10-17
- **Spec/ADR Path**: `docs/spec/0002-testing-and-performance-harness.md`, `docs/adr/0005-extract-blackhole-core.md`
- **Ticket Path**: `docs/archive/tickets/0010-headless-render.md`
- **Branch**: `main`

---

## 📌 Overview
- **Commits**: 0 (uncommitted working-tree changes vs `HEAD` `23db6c2`)
- **Files Changed**: 9 tracked (`+156` / `-53`) plus 5 new files (4 sources + `tests/gpu/`)

## 📝 Key Changes
- **Observation seam**: new `--render <config.json> --out <file>` entry point renders one frame with a **hidden** window, reads back the post-tone-map `RGBA32F` compute texture, writes raw little-endian `float32` RGBA (`W·H·4`) + a `<file>.meta` shape sidecar, and exits. Exit `0` = ok, `1` = failure, `2` = skipped (no GL 4.3 compute).
- **`RayTracer::readback(std::vector<float>&) const`** (`include/rendering/ray_tracer.h`, `src/rendering/ray_tracer.cpp`): `glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, …)` on `outputTexture_` (the compute image, not the back-buffer), with stale-error drain.
- **`Window(…, visible = true)`** (`include/app/window.h`, `src/app/window.cpp`): `GLFW_VISIBLE=FALSE` for headless; interactive path unchanged.
- **Shared `buildRayTracerParams(const Config&)`** (`include/app/render_params.*`): extracts the config→`RayTracerParams` mapping so interactive and headless cannot drift (CONTEXT §5.3).
- **`runHeadless`** (`include/app/headless.*`): config → hidden window → capability gate → one dispatch → `glFinish` → readback → write.
- **CLI** (`src/main.cpp`): no args = interactive viewer; `--help`; `--out` without `--render` is rejected.
- **Test** (`tests/gpu/headless_check.py`, stdlib only): renders twice, validates the buffer against its sidecar (`bytes == W·H·4·4`), asserts byte-identical + non-degenerate frame. Registered as `test_headless_render` with `SKIP_RETURN_CODE 2`; missing `python3` now warns instead of silently dropping the check.
- `docs/CONTEXT.md` §2/§3/§5/§6 updated; ticket 0010 marked `Status: Done`.

## 📊 Verification
- **Typecheck**: 🟢 `cmake --build build -j` clean (core + app + tests)
- **Test suite**: 🟢 `ctest` 5/5 passed, total ~1.6 s
- **Headless render**: 🟢 512×512 in ~0.3 s (D3D12/RTX 4070), 4 194 304-byte buffer + sidecar; 1 004 488/1 048 576 floats non-zero, max 1.0
- **Honest skip**: 🟢 missing `DISPLAY` → exit 2 `SKIPPED`; bad config → exit 1
- **Interactive unchanged**: 🟢 `timeout 8` run stays alive rendering; same `RayTracer`/params

## 🔍 Code Review Findings
- **Standards**: Byte-exact determinism is a hard break against `testing-and-performance.md` §1 ("tolerance for accelerators") — resolved as the *deterministic-path* case with an in-file justification. Sidecar omitted byte order/orientation (fixed). Ticket 0010 marked Done without archiving + no `CHANGELOG.md` entry (pre-existing debt, also 0008/0009). Baseline smells: `buildRayTracerParams` looked like a Middle Man but is the documented anti-drift translation; `--help` a weak Speculative Generality.
- **Spec**: All acceptance criteria met. Findings: the shape sidecar was written but never verified (fixed — check now asserts the declared shape); `--render <config.json>` is optional/relaxed vs the ticket signature; `--out` without `--render` was silently ignored (fixed); the check is gated on `python3`, so it could vanish silently (fixed with a warning); `std::printf`/`fprintf` used without `<cstdio>` (fixed).
- **Addressed in follow-up**: `<cstdio>` include; sidecar `byte_order`/`origin`; sidecar validation in the check; `python3`-missing warning; `--out`-without-`--render` error; documented the determinism-path justification.
- **Deferred**: ticket archival + `CHANGELOG.md` (lifecycle close, also pending for 0008/0009); distinguishing a software (llvmpipe) renderer from a GPU reference — belongs to ticket 0011.

## 📦 Recent Commits
- `23db6c2` feat(tests): add CPU unit tests for metric, integrator, and config; update documentation
- `5083202` feat(build): extract blackhole_core library and add CTest support with initial tests

## ⏭️ Follow-ups
- Close lifecycle for 0008/0009/0010: archive tickets, update `CHANGELOG.md`.
- Next ticket in the milestone: **0011** (golden regression harness + first references), which builds the comparator and reference set on top of this `--render` seam. Ticket 0011's acceptance also wires the determinism check that 0010 introduced.
