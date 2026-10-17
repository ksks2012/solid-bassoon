# Execution Summary: 0008-extract-core-library

- **Date**: 2026-10-16
- **Spec/ADR Path**: `docs/spec/0002-testing-and-performance-harness.md`, `docs/adr/0005-extract-blackhole-core.md`
- **Ticket Path**: `docs/ticket/0008-extract-core-library.md`
- **Branch**: `main`

---

## 📌 Overview
- **Commits**: 0 (uncommitted working-tree changes vs `HEAD` `f5f9244`)
- **Files Changed**: 6 files (`+142` / `-52`)

## 📝 Key Changes
- Split the build: added a **GL-free static library `blackhole_core`** (`src/core/*`, `src/app/config.cpp`, `src/logging/spdlog_logger.cpp`) with `PUBLIC` include roots; `BlackholeRenderer` now links it and keeps `main`/`window`/`rendering`/ImGui.
- Moved from global `include_directories`/`add_definitions` to **target-scoped** includes and `IMGUI_IMPL_OPENGL_LOADER_CUSTOM`; dropped the stale `src/physics/*` glob.
- Added a committed, dependency-free test harness **`tests/check.h`** plus a `test_smoke` executable wired into **CTest** (`enable_testing()` + `tests/CMakeLists.txt`).
- Updated CONTEXT §2/§3/§6/§7 to describe the `blackhole_core` target, the `tests/` layout, and the current "skeleton only" testing status; marked ticket 0008 **Done**.

## 🧪 Verification
- **Typecheck**: 🟢 Pass (`cmake --build build -j`, no warnings introduced)
- **Tests**: 🟢 Pass (`ctest`: 1/1, `test_smoke` passes)
- **No GL in core**: 🟢 `flags.make` shows only `include/`, spdlog, nlohmann — no GL/GLEW/GLFW/imgui
- **No behaviour change**: 🟢 app runs; startup log byte-identical to the pre-refactor run (timestamps stripped)

## 🔍 Code Review Findings
- **Standards**: No CONTEXT §5 / ADR-0005 hard-rule violations (verified: core is GL-free; tests link `blackhole_core` only; include-dir migration complete). Documented-lifecycle gaps flagged: ticket marked Done without archiving to `docs/archive/tickets/` and without a `CHANGELOG.md` update; spec 0002's ticket table not updated. Smells: `CHECK_CLOSE` unused (possible Speculative Generality).
- **Spec**: Criteria 1–3, 5 met. Criterion 4 "renders identically" verified out-of-band (log diff), not captured as an artifact. Two findings: (i) no target literally named `tests` (a `tests/` dir with `test_smoke`); (ii) **the include tree is not partitioned by layer** — `blackhole_core` advertises `include/`, which also holds GL-bearing headers (`rendering/ray_tracer.h`, `app/window.h`), so the ADR's "public headers stay runtime-free" invariant is only partially enforced.

## 📦 Recent Commits
- (none — changes are staged, not committed)

## ⏭️ Follow-ups (from review)
- Close per lifecycle: archive ticket 0008 to `docs/archive/tickets/`, fix the spec 0002 link, add a `CHANGELOG.md` entry.
- Consider per-layer include roots (e.g. `include/core`, `include/logging`) to fully enforce the ADR-0005 invariant.
- Trim or keep `CHECK_CLOSE` — ticket 0009 (tolerances) will use it.
