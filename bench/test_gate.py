#!/usr/bin/env python3
"""Self-verification of the performance gate (ticket 0013, bench/gate.py).

The gate's decision logic is exercised on synthetic rows and synthetic
baselines, exactly like tests/gpu/test_compare.py does for the golden
comparator: boundaries of the ±10 % band, the budget rules, the honest-skip
policy, and CSV parsing of renderer output. No GPU required, so this runs on
every machine while `test_perf_gate` is the one that touches hardware.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gate  # noqa: E402

FAILURES = []


def check(cond, what):
    if not cond:
        FAILURES.append(what)
        print("  FAIL {}".format(what), file=sys.stderr)


def approx(a, b, tol=1e-9):
    return abs(a - b) <= tol


def row(**overrides):
    """A complete, valid `--bench` row as gate.parse_bench_csv would return it."""
    base = {
        "median_ms": 10.0,
        "p95_ms": 11.0,
        "mean_ms": 10.1,
        "min_ms": 9.8,
        "fps": 100.0,
        "renderer": "D3D12 (NVIDIA GeForce RTX 4070)",
        "steps_per_ray": 812.5,
        "stats_median_ms": 10.5,
        "stats_overhead_pct": 5.0,
        "width": 512,
        "height": 512,
        "max_steps": 5000,
        "jet_enabled": 0,
        "disk_enabled": 1,
        "frames": 20,
        "warmup": 5,
    }
    base.update(overrides)
    return base


def baseline_case(**overrides):
    base = {
        "name": "disk",
        "config": "etc/config.json",
        "median_ms": 10.0,
        "p95_ms": 11.0,
        "steps_per_ray": 812.5,
        "width": 512,
        "height": 512,
        "max_steps": 5000,
        "jet_enabled": 0,
        "disk_enabled": 1,
    }
    base.update(overrides)
    return base


def main():
    # ── The ±10 % band: in-band and out-of-band on both sides ────────────────
    verdict, ratio = gate.evaluate(11.0, 10.0)          # exactly +10 %
    check(verdict == "ok", "evaluate: exactly +10% is ok, got " + verdict)
    check(approx(ratio, 1.1), "evaluate: ratio 1.1")

    verdict, _ = gate.evaluate(11.01, 10.0)             # just past +10 %
    check(verdict == "regression", "evaluate: +10.01% is a regression")

    verdict, _ = gate.evaluate(9.0, 10.0)               # exactly -10 %
    check(verdict == "ok", "evaluate: exactly -10% is ok, got " + verdict)

    verdict, _ = gate.evaluate(8.99, 10.0)              # just past -10 %
    check(verdict == "improvement",
          "evaluate: -10.01% is a suspicious improvement, got " + verdict)

    # A suspicious improvement is flagged, not celebrated: it is as likely to
    # be a broken measurement as a genuine win.
    check(gate.evaluate(5.0, 10.0)[0] == "improvement",
          "evaluate: 2x faster is still flagged")

    # ── Measurement that cannot be trusted ───────────────────────────────────
    check(gate.evaluate(0.0, 10.0)[0] == "invalid",
          "evaluate: zero median is invalid")
    check(gate.evaluate(float("nan"), 10.0)[0] == "invalid",
          "evaluate: NaN median is invalid")
    try:
        gate.evaluate(10.0, 0.0)
        check(False, "evaluate: zero baseline raises instead of dividing")
    except ValueError:
        check(True, "evaluate: zero baseline raises")

    # ── CSV parsing out of interleaved renderer logs ─────────────────────────
    header = gate.csv_header()
    log = "\n".join([
        "2026-10-20 12:00:00 [info] compute dispatch completed",
        header,
        '10.125,11.5,10.33,9.875,98.765432,"D3D12 (NVIDIA GeForce RTX 4070)",'
        "812.5,10.5,3.6,512,512,5000,1,0,20,5",
        "2026-10-20 12:00:01 [info] done",
    ])
    parsed = gate.parse_bench_csv(log)
    check(parsed is not None, "parse: header found among log lines")
    if parsed is not None:
        check(approx(parsed["median_ms"], 10.125), "parse: median_ms")
        check(approx(parsed["p95_ms"], 11.5), "parse: p95_ms")
        check(approx(parsed["fps"], 98.765432), "parse: fps")
        check(parsed["renderer"] == "D3D12 (NVIDIA GeForce RTX 4070)",
              "parse: renderer")
        check(parsed["steps_per_ray"] == 812.5, "parse: steps_per_ray")
        check(parsed["width"] == 512 and parsed["max_steps"] == 5000,
              "parse: integer fields")
        check(parsed["jet_enabled"] == 1 and parsed["disk_enabled"] == 0,
              "parse: boolean fields as 0/1")
        check(parsed["frames"] == 20 and parsed["warmup"] == 5,
              "parse: frames/warmup")

    # A row that is short of the header is worse than no row: it parses into
    # the wrong columns, so it must be refused.
    short_log = header + "\n10.0,11.0,10.1,9.9,100.0,GPU\n"
    check(gate.parse_bench_csv(short_log) is None, "parse: short row refused")

    check(gate.parse_bench_csv("no csv here\n") is None,
          "parse: missing header returns None")
    check(gate.parse_bench_csv(header + "\n") is None,
          "parse: header without a row returns None")

    # ── Derived diagnostic: jet marginal cost ────────────────────────────────
    check(approx(gate.jet_marginal_pct(11.5, 10.0), 15.0),
          "jet_marginal_pct: +15%")
    check(approx(gate.jet_marginal_pct(10.0, 10.0), 0.0),
          "jet_marginal_pct: no cost is 0")
    check(gate.jet_marginal_pct(10.0, 0.0) == float("inf")
          or gate.jet_marginal_pct(10.0, 0.0) > 0,
          "jet_marginal_pct: zero baseline does not crash")

    # ── Per-case verdicts ────────────────────────────────────────────────────
    budgets = {"full_frame_ms": 12.0, "budget_width": 512, "budget_height": 512,
               "jet_marginal_pct": 15.0, "jet_pair": ["disk", "disk_jet"]}

    findings = gate.evaluate_case("disk", row(), baseline_case(), budgets=budgets)
    check(findings == [], "evaluate_case: in-band, in-budget case passes")

    findings = gate.evaluate_case("disk", row(median_ms=11.0),
                                  baseline_case(), budgets=budgets)
    check(findings == [], "evaluate_case: exactly +10% passes")

    findings = gate.evaluate_case("disk", row(median_ms=11.5),
                                  baseline_case(), budgets=budgets)
    check(any("regression" in f for f in findings),
          "evaluate_case: +15% is a regression, got {!r}".format(findings))

    findings = gate.evaluate_case("disk", row(median_ms=8.0),
                                  baseline_case(), budgets=budgets)
    check(any("improvement" in f for f in findings),
          "evaluate_case: -20% is a suspicious improvement")

    findings = gate.evaluate_case("disk", row(median_ms=13.0),
                                  baseline_case(median_ms=20.0),
                                  budgets=budgets)
    check(any("budget" in f for f in findings),
          "evaluate_case: 13 ms over a 12 ms budget is a finding")
    check(not any("regression" in f for f in findings),
          "evaluate_case: slower than baseline but under baseline is only a "
          "budget finding")

    # Off-budget resolutions are recorded but not gated by the reference-GPU
    # budgets — the budget's own configuration defines where it applies.
    off = row(median_ms=30.0, width=1024, height=1024)
    findings = gate.evaluate_case("hi_res", off,
                                  baseline_case(median_ms=28.0, width=1024,
                                                height=1024),
                                  budgets=budgets)
    check(findings == [], "evaluate_case: 1024x1024 is not held to 512 budget")

    # No usable measurement and a missing derived diagnostic are failures, not
    # zeros to be compared against a baseline.
    findings = gate.evaluate_case("disk", row(median_ms=0.0), baseline_case(),
                                  budgets=budgets)
    check(any("no valid samples" in f for f in findings),
          "evaluate_case: zero median is reported, not compared")

    findings = gate.evaluate_case("disk", row(steps_per_ray=0.0),
                                  baseline_case(), budgets=budgets)
    check(any("steps_per_ray" in f for f in findings),
          "evaluate_case: missing RK4 diagnostic is a finding")

    # A baseline case that never recorded a median cannot be compared to.
    try:
        gate.evaluate_case("disk", row(), baseline_case(median_ms=0.0),
                           budgets=budgets)
        check(False, "evaluate_case: zero baseline raises")
    except ValueError:
        check(True, "evaluate_case: zero baseline raises")

    # ── Sampling protocol ───────────────────────────────────────────────────
    # A median taken over a different number of samples — or after a different
    # warm-up — is not comparable with the recorded one, however honest it is.
    check(gate.protocol_drift(20, 5, 20, 5) is None,
          "protocol_drift: identical protocol is not a finding")
    drift = gate.protocol_drift(3, 1, 20, 5)
    check(drift is not None and "protocol drift" in drift
          and "warmup=1 frames=3" in drift,
          "protocol_drift: shorter run is reported")
    check(gate.protocol_drift(20, 5, None, None) is not None,
          "protocol_drift: baseline with no recorded protocol is reported")

    # ── The cross-language CSV header contract ─────────────────────────────
    # The header exists twice in the codebase (this file's gate, and the C++
    # bench::csvHeader). Each side pins the SAME literal, so renaming one field
    # on one side fails the other side's test instead of silently re-aligning
    # every column for every reader.
    expected_header = (
        "median_ms,p95_ms,mean_ms,min_ms,fps,renderer,steps_per_ray,"
        "stats_median_ms,stats_overhead_pct,width,height,max_steps,"
        "jet_enabled,disk_enabled,frames,warmup"
    )
    check(gate.CSV_HEADER == expected_header,
          "CSV_HEADER pinned literally, got {!r}".format(gate.CSV_HEADER))
    check(gate.parse_bench_csv(gate.CSV_HEADER + "\n1,2,3,4,5,x,7,8,9,10,11,"
                               "12,13,14,15,16") is not None,
          "CSV_HEADER parses as a 16-field row")

    # ── Hardware other than the baseline's: report, never judge ───────
    reference = "D3D12 (NVIDIA GeForce RTX 4070)"
    check(gate.environment_finding(reference, reference) is None,
          "environment_finding: the reference GPU is not a finding")
    check(gate.environment_finding(None, reference) is None
          and gate.environment_finding(reference, None) is None,
          "environment_finding: an unknown renderer is never a verdict")
    other = gate.environment_finding("llvmpipe (LLVM 15.0.7)", reference)
    check(other is not None and "llvmpipe" in other and reference in other
          and "not comparable" in other,
          "environment_finding: a mismatch names both renderers and says why")

    # ── The jet pair: over budget, and its mirror image ───────────────
    check(gate.jet_budget_findings(1.4, 15.0, 0.10, "jet_on", "disk") == [],
          "jet_budget_findings: an in-budget marginal is silent")
    over = gate.jet_budget_findings(20.0, 15.0, 0.10, "jet_on", "disk")
    check(len(over) == 1 and "exceeds" in over[0],
          "jet_budget_findings: over the budget is a finding, got {!r}".format(over))
    check(gate.jet_budget_findings(-5.0, 15.0, 0.10, "jet_on", "disk") == [],
          "jet_budget_findings: a -5% marginal is inside the band")
    fast = gate.jet_budget_findings(-12.0, 15.0, 0.10, "jet_on", "disk")
    check(len(fast) == 1 and "suspicious improvement" in fast[0],
          "jet_budget_findings: a far faster jet is suspicious, not a pass")
    check(gate.jet_budget_findings(None, 15.0) == [],
          "jet_budget_findings: no measurement, no verdict")

    # ── One source of findings: the two severities never mix ───────────
    nan_row = row(median_ms=float("nan"))
    nan_errs, _ = gate.split_findings("disk", nan_row, baseline_case(), 0.10)
    check(len(nan_errs) == 1 and "no valid samples" in nan_errs[0],
          "split_findings: a NaN median is an error")
    slow_errs, slow_cmps = gate.split_findings("disk", row(median_ms=12.0),
                                               baseline_case(), 0.10)
    check(slow_errs == [] and len(slow_cmps) == 1
          and "regression" in slow_cmps[0],
          "split_findings: a regression is a comparison, not an error")
    no_cmp_errs, no_cmp_cmps = gate.split_findings("disk", row(median_ms=12.0),
                                                   baseline_case(), 0.10,
                                                   with_comparisons=False)
    check(no_cmp_errs == [] and no_cmp_cmps == [],
          "split_findings: with_comparisons=False drops only the comparison")
    still_errs, _ = gate.split_findings("disk", nan_row, baseline_case(), 0.10,
                                        with_comparisons=False)
    check(still_errs == nan_errs,
          "split_findings: errors survive with_comparisons=False")
    check(gate.evaluate_case("disk", row(median_ms=12.0), baseline_case(),
                             0.10) == slow_cmps,
          "evaluate_case: reports exactly what split_findings returns")

    # ── Aggregate decision: fail is never masked by a skip ───────────────────
    check(gate.exit_code([], None) == 0, "exit_code: clean run passes")
    check(gate.exit_code([], "different renderer") == 2,
          "exit_code: mismatched GPU skips")
    check(gate.exit_code(["slow"], "different renderer") == 1,
          "exit_code: a failure is never masked by a later skip")
    check(gate.exit_code([], "unsupported driver") == 2,
          "exit_code: honest skip")

    # ── Baseline validation: a malformed baseline must fail loudly ───────────
    full_baseline = {
        "schema": 1,
        "environment": {"gl_renderer": "D3D12 (NVIDIA GeForce RTX 4070)",
                        "note": "recorded"},
        "tolerance": 0.10,
        "budgets": dict(budgets),
        "jet_pair": ["disk", "disk_jet"],
        "cases": [baseline_case()],
    }
    check(gate.validate_baseline(full_baseline) is None,
          "validate_baseline: complete baseline is valid")
    broken = dict(full_baseline)
    del broken["cases"]
    check(gate.validate_baseline(broken) is not None,
          "validate_baseline: missing cases is an error")
    broken = dict(full_baseline)
    broken["cases"] = [{"name": "disk"}]  # no median_ms recorded
    check(gate.validate_baseline(broken) is not None,
          "validate_baseline: case without median_ms is an error")
    broken = dict(full_baseline)
    broken["tolerance"] = 0.5  # a 50 % "band" would gate nothing
    check(gate.validate_baseline(broken) is not None,
          "validate_baseline: tolerance above 10% is an error")
    broken = dict(full_baseline)
    broken["cases"] = []
    check(gate.validate_baseline(broken) is not None,
          "validate_baseline: zero cases is an error")

    # ── Re-recording measurements must never redefine the policy ─────────────
    # A budget or a band that could be rewritten by the same command that
    # measures it would not gate anything. --update replaces the numbers only.
    previous = {
        "schema": 1,
        "environment": {"gl_renderer": "OLD GPU", "host": "box-1"},
        "tolerance": 0.08,
        "budgets": {"full_frame_ms": 9.5, "budget_width": 512,
                    "budget_height": 512, "jet_marginal_pct": 15.0,
                    "jet_pair": ["disk", "jet_on"]},
        "frames": 30,
        "warmup": 7,
        "cases": [baseline_case()],
        "reviewed_by": "someone-who-signed-off",
    }
    measured = [{"name": "disk", "config": "etc/config.json",
                 "actual": row(median_ms=9.25, p95_ms=9.5,
                               steps_per_ray=800.0)}]
    updated = gate.build_baseline(previous, measured, "NEW GPU",
                                  "re-measured after commit abc", 20, 5)
    check(updated["tolerance"] == 0.08,
          "build_baseline: tolerance carried over, not reset")
    check(updated["budgets"] == previous["budgets"],
          "build_baseline: budgets carried over untouched")
    check(updated.get("reviewed_by") == "someone-who-signed-off",
          "build_baseline: unrelated keys survive an update")
    check(updated["frames"] == 20 and updated["warmup"] == 5,
          "build_baseline: frame counts come from the run")
    check(updated["environment"] == {"gl_renderer": "NEW GPU", "host": "box-1"},
          "build_baseline: environment updated, rest kept")
    check(len(updated["cases"]) == 1
          and updated["cases"][0]["median_ms"] == 9.25
          and updated["cases"][0]["steps_per_ray"] == 800.0,
          "build_baseline: measurements replaced")
    check(updated["last_update"]["reason"] == "re-measured after commit abc",
          "build_baseline: reason recorded")
    check(gate.validate_baseline(updated) is None,
          "build_baseline: result is a valid baseline")

    # ── Runner refusals that never reach the GPU ──────────────────────────────
    # Both paths have to fail before any subprocess is spawned, and loudly:
    # "the baseline is gone" and "you forgot to say why" are configuration
    # mistakes, not performance results.
    import contextlib
    import io as _io

    missing = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           "no_such_baseline.json")
    with contextlib.redirect_stdout(_io.StringIO()), \
            contextlib.redirect_stderr(_io.StringIO()) as err:
        rc_missing = gate.main(["--renderer", "/nonexistent",
                                "--baseline", missing])
    check(rc_missing == 1, "main: missing baseline fails, got {}".format(rc_missing))
    check("seed it" in err.getvalue(),
          "main: missing baseline explains itself")

    with contextlib.redirect_stdout(_io.StringIO()), \
            contextlib.redirect_stderr(_io.StringIO()) as err:
        rc_reason = gate.main(["--renderer", "/nonexistent",
                               "--baseline", missing, "--update"])
    check(rc_reason == 1, "main: --update without --reason fails, got {}".format(rc_reason))
    check("--reason" in err.getvalue(),
          "main: missing --reason explains itself")

    if FAILURES:
        print("FAIL test_bench_gate ({} check(s) failed)".format(len(FAILURES)),
              file=sys.stderr)
        return 1
    print("PASS test_bench_gate")
    return 0


if __name__ == "__main__":
    sys.exit(main())
