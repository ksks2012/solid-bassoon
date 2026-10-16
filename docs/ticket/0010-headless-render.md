# 0010 — Headless `--render` entry point + output readback

**Spec:** [../spec/0002-testing-and-performance-harness.md](../spec/0002-testing-and-performance-harness.md)

**What to build:** A headless automation entry point on the executable (`--render <config.json> --out <file>`) that initialises a hidden window, dispatches the compute shader once, reads back the **post-tone-map `RGBA32F`** output texture, writes it, and exits. Add a thin `RayTracer::readback` accessor. This is the observation seam for all regression tests.

**Blocked by:** 0008 — can start once the core library and build split exist.

**Status:** Todo

- [ ] `--render` renders one frame headlessly, writes the output, and exits with code 0.
- [ ] The readback uses `glGetTexImage` on the compute output texture (not the window back-buffer), preserving the `NEAREST`-filtered values.
- [ ] Two runs with identical inputs produce identical buffers (determinism).
- [ ] When no GL 4.3 + compute context is available, the mode fails loudly or reports `skipped` — never a nominally successful render it did not produce.
- [ ] Interactive behaviour of the app is unchanged.

**See:** [../standards/testing-and-performance.md](../standards/testing-and-performance.md) · [../adr/0005-extract-blackhole-core.md](../adr/0005-extract-blackhole-core.md)
