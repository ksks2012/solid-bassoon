# 0010 — Headless `--render` entry point + output readback

**Spec:** [../spec/0002-testing-and-performance-harness.md](../spec/0002-testing-and-performance-harness.md)

**What to build:** A headless automation entry point on the executable (`--render <config.json> --out <file>`) that initialises a hidden window, dispatches the compute shader once, reads back the **post-tone-map `RGBA32F`** output texture, writes it, and exits. Add a thin `RayTracer::readback` accessor. This is the observation seam for all regression tests.

**Blocked by:** 0008 — can start once the core library and build split exist.

**Status:** Done · Completed: 2026-10-17

- [x] `--render <config.json> --out <file>` renders one frame headlessly (hidden window via `Window(..., visible=false)`), writes the output, and exits `0`. Measured: 512×512 in ~0.3 s on the D3D12/RTX 4070 path.
- [x] `RayTracer::readback(std::vector<float>&)` uses `glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, …)` on `outputTexture_` (the compute image), not the window back-buffer; `NEAREST` filtering is preserved. Output = raw little-endian `float32` RGBA (`W·H·4` floats) + a `<file>.meta` shape sidecar.
- [x] Two runs with identical inputs produce byte-identical buffers — enforced by `tests/gpu/headless_check.py` under CTest (`test_headless_render`).
- [x] No GL 4.3 + compute context → the mode prints `SKIPPED: …` to stderr and exits `2`; the CTest test declares `SKIP_RETURN_CODE 2`, so capability gaps surface as **skipped**, never a false pass. Config/IO errors exit `1`.
- [x] Interactive behaviour is unchanged: the interactive path loads `etc/config.json` in a visible window as before, and now shares `buildRayTracerParams(config)` with the headless path.
- [x] `main.cpp` gains a minimal CLI (`--render`, `--out`, `--help`); `app/render_params.*` centralises the config→`RayTracerParams` mapping.
- [x] `ctest` green: `test_smoke` / `test_metric` / `test_integrator` / `test_config` / `test_headless_render` (5/5).

**See:** [../standards/testing-and-performance.md](../standards/testing-and-performance.md) · [../adr/0005-extract-blackhole-core.md](../adr/0005-extract-blackhole-core.md)
