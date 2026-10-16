# Testing & Performance Standards

A standard for **how software in this project is verified** and **how its
performance is measured**, following the same spirit as
[DOCUMENTATION-LIFECYCLE.md](../DOCUMENTATION-LIFECYCLE.md). It is intentionally
**project-agnostic**: it states the principles, layers, and protocols, not any
particular implementation. Concrete seams, scenarios, thresholds, and budgets
belong in a spec and its tickets, not here.

---

## 1. Principles Overview

### Test by layer, cheapest first

Verification is organized as a pyramid: many fast, exact checks of pure logic at
the base; few slow, observational checks of the whole system at the top. Each
layer answers a different question and differs in cost and determinism.

| Layer | Answers | Cost | Determinism | Requires |
|---|---|---|---|---|
| Unit | "Is this pure logic correct?" | milliseconds | exact | any machine |
| Component probe | "Is this isolated runtime function correct?" | seconds | tolerant | capable runtime |
| Regression (golden) | "Did the observable output change?" | seconds–minutes | tolerant | capable runtime |
| Cross-validation | "Do two implementations of the same thing agree?" | seconds | tolerant | capable runtime |
| Performance | "Did the cost change?" | minutes | noisy | reference machine |

### Seams before tests

A unit cannot be tested until it can be reached without unrelated heavy
dependencies. Pure logic must be **linkable and callable in isolation** from the
runtime that surrounds it. Observation must happen at a **stable, deterministic
boundary** — not at the presentation edge, where buffering, filtering, or
colour encoding can perturb the result.

### Determinism and tolerance

Exact equality is asserted only where the computation is genuinely exact (pure
integer/rational logic, conservation laws, invariants). Anything involving
floating point, parallelism, or an accelerator is compared within a **tolerance
justified by the scenario**. Never conflate "close enough" with "equal".

### Correctness-normalized performance

A performance number is meaningless on its own. Every performance result is
valid **only if the corresponding correctness checks pass**. A faster system
that produces a different result is not an improvement.

### Honest results

A check that could not run is reported as **skipped**, never as passed. Missing
capability must be visible, not silently green.

---

## 2. Standard Structure

Recommended generic layout:

```
tests/
├── support/          # harness and comparison tooling (committed)
├── unit/             # pure logic; no heavy runtime
├── component/        # isolated runtime functions
├── regression/       # golden / reference comparisons
│   └── reference/    # committed references + metadata
└── ...
bench/
└── baseline.*        # committed performance baseline (gate input)
```

### Reference (golden) format

Store each reference in two forms:

* a **comparison form** — a lossless representation of the observed output (for
  example, raw floats) on which the diff is computed;
* a **viewing form** — a compressed or human-readable copy (for example, an
  image) used only for inspection.

Record the expected shape and metadata (resolution, channel count, byte order,
stride) in a sidecar so that a mismatch **fails loudly** instead of silently
comparing the wrong bytes. A comparison reports, at minimum:

* maximum absolute difference,
* mean absolute difference,
* fraction of elements above a threshold.

### Naming and metadata

Each test artifact states its **layer**, the **seam** it observes, and its
**tolerance**. Regression references change only with an explicit, recorded
reason.

---

## 3. Workflow Checklist

* [ ] **Before writing tests**: identify the seam; ensure the logic is reachable
      in isolation; add an automation entry point if the system is interactive.
* [ ] Prefer the **cheapest layer** that can catch the defect.
* [ ] Assert exact equality **only** for genuinely exact computations.
* [ ] **Justify every tolerance** in the test.
* [ ] When cross-validating two implementations, assert agreement on
      unambiguous cases and allow a **documented ambiguity band** where their
      intentional differences live.
* [ ] **Golden/reference update**: regenerate, inspect the diff, record the
      reason, and replace the reference in the **same change** as the code. A
      reference that changes without a code change is a bug.
* [ ] **Performance**: warm up, report the **median and a tail percentile**
      (never a lone mean), and pin or record the environment.
* [ ] Gate on a **committed baseline** with an explicit threshold; flag both
      regressions and suspicious improvements.
* [ ] **In CI**, report skipped capability honestly; never fake a pass.
* [ ] Keep **this** document abstract: move concrete seams, scenarios, and
      budgets into a spec and its tickets.

---

## 4. Threats to Validity & Mitigations

| Threat | Mitigation |
|---|---|
| Representation mismatch (precision, accelerator vs reference) | compare with justified tolerances; exact only for exact logic |
| Run-to-run nondeterminism | require identical output where the path is deterministic; otherwise tolerate and quantify |
| Environment / thermal / clock variance | warm up; report median & tail; pin or record the environment |
| Reference rot as the intended output evolves | references are versioned; change them only by explicit intent |
| Missing capability (no runtime / no accelerator) | report **skipped**, never pass |
| Noise mistaken for a change | state the noise floor; ignore changes below it |

---

## 5. AI Assistant Guidance

1. **Read the core context first**; find the seams before proposing any tests.
2. Start with the **cheapest layer** that can catch the defect.
3. Never present a **skipped or unrun** check as passing.
4. Treat references as **reviewed artifacts**, not regenerable-by-default.
5. Keep this standard **abstract**; project-specific choices live in the
   relevant spec, ADRs, and tickets.
