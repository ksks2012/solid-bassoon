# 0008 — Extract `blackhole_core` test library + CTest skeleton

**Spec:** [../spec/0002-testing-and-performance-harness.md](../spec/0002-testing-and-performance-harness.md)

**What to build:** Extract the pure, GL-free sources (`SchwarzschildMetric`, `GeodesicIntegrator`, `Config`, logging) into a static library `blackhole_core`, keep the executable for everything that needs the runtime, and introduce a CTest test target that links the core only. Add a minimal committed assertion header (`tests/check.h`). No behaviour change to the app.

**Blocked by:** —

**Status:** Done
**Completed:** 2026-10-16

- [x] `CMakeLists.txt` builds `blackhole_core`, `BlackholeRenderer`, and a `tests` target that links `blackhole_core` only.
- [x] `src/core/*`, `src/app/config.cpp`, `src/logging/*` compile without GL/GLFW/ImGui includes or flags.
- [x] `ctest` runs at least one trivial passing test.
- [x] The application still builds and renders identically (no behaviour change).
- [x] CONTEXT §3 and §7 describe the library target; [adr/0005](../adr/0005-extract-blackhole-core.md) recorded.

**See:** [../standards/testing-and-performance.md](../standards/testing-and-performance.md) · [../adr/0005-extract-blackhole-core.md](../adr/0005-extract-blackhole-core.md)
