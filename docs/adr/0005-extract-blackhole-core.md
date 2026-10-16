# ADR-0005 — Extract a GL-free `blackhole_core` library

- **Status:** Accepted
- **Date:** 2026-10-16
- **References:** [../spec/0002-testing-and-performance-harness.md](../spec/0002-testing-and-performance-harness.md) · [../standards/testing-and-performance.md](../standards/testing-and-performance.md)

## Context

Every source file is currently globbed into a single executable
(`CMakeLists.txt`), so the pure core — `SchwarzschildMetric`,
`GeodesicIntegrator` and `Config` — is compiled only as part of a binary that
also links OpenGL, GLFW and ImGui. There is no way to call this logic from a
test without pulling in a GL context. With no automated tests (CONTEXT §2, §6),
correctness rests on a single CPU trace printed at startup and on manual image
inspection.

The [testing standard](../standards/testing-and-performance.md) requires that
pure logic be linkable and callable **in isolation** ("seams before tests"). The
base of the test pyramid — unit tests for the metric, the integrator and the
config parser — is unreachable until that seam exists.

## Decision

Extract a **GL-free static library `blackhole_core`** containing the pure,
runtime-independent sources:

- `src/core/metric.cpp`
- `src/core/geodesic_integrator.cpp`
- `src/app/config.cpp`
- `src/logging/spdlog_logger.cpp`

Logging is included because `Config` reaches the logger through `GlobalLogger`;
if the core is purified further later, its `ILogger` interface can be split out
so the core depends only on the interface.

The application target (`BlackholeRenderer`) keeps everything that needs the
runtime — `main.cpp`, `window.cpp`, `ray_tracer.cpp`, the vendored ImGui sources
— and links `blackhole_core`. Test targets link **`blackhole_core` only**.

A hard rule accompanies the split: **`blackhole_core` must not include or
depend on GL/GLFW/ImGui headers.** Its public headers stay runtime-free.

## Alternatives considered

- **Test through the executable (black-box).** Rejected: it gives no access to
  the individual units, and every unit test would need a working runtime.
- **Keep one target and add a test target that links GL.** Rejected: unit tests
  for pure math would still require a GL context, defeating the cheap/exact base
  layer and breaking on machines without one.
- **Vendor a test framework under `lib/`.** Rejected for the framework: `lib/`
  is gitignored, so a framework there is not reproducible. The harness lives in
  `tests/` and is committed.

## Consequences

- Test layers 1–3 (unit, component, regression) and the headless entry point
  become possible; this is the **blocking prerequisite** for the harness.
- The build is restructured: include directories and compile flags are attached
  per target rather than globally (`include_directories` → target-scoped).
- Keeping the core GL-free is now a maintained invariant; a GL include leaking
  into it is a defect.
- CONTEXT §3 (layers) and §7 (directory layout) must be updated to describe the
  library target when this lands.
