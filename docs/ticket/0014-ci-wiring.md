# 0014 — CI wiring with honest skip reporting

**Spec:** [../spec/0002-testing-and-performance-harness.md](../spec/0002-testing-and-performance-harness.md)

**What to build:** Wire the layers into continuous integration so that cheap, exact checks always run and capability-dependent checks run only where a capable runtime exists — reporting `skipped`, never a fake pass, when it does not.

**Blocked by:** 0009, 0011, 0013 — needs unit tests, golden regression, and the benchmark to wire.

**Status:** Todo

- [ ] CI builds the project and runs the unit tests on every push.
- [ ] Golden regression and the benchmark run only where a GL 4.3 + compute context exists; otherwise they are reported as **skipped**.
- [ ] A missing GPU can never produce a green pass (the skip path is explicit and visible).
- [ ] A separate slow reference set (software renderer) is used where the GPU path is unavailable, rather than pretending the outputs match.
- [ ] Diffs and the baseline are retained as artifacts for inspection.

**See:** [../standards/testing-and-performance.md](../standards/testing-and-performance.md) · [../adr/0005-extract-blackhole-core.md](../adr/0005-extract-blackhole-core.md)
