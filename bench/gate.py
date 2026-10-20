#!/usr/bin/env python3
"""Performance gate: `--bench` results against a committed baseline
(ticket 0013, spec/0002 §"Performance protocol", standard §4).

Two jobs, deliberately separated:

  * **decision logic** at the top of this file — pure, no I/O, imported and
    verified by bench/test_gate.py without a GPU;
  * **runner** (`main()`) at the bottom — renders, times, compares, reports.

The measurement itself is the renderer's (`--bench`); this file only decides
what the numbers mean. Policy that must be reviewable in a diff — the ±10 % band,
the reference-GPU budgets, the configuration matrix — lives in the committed
`bench/baseline.json`, never in this script, and updating measurements preserves
it: a budget cannot be re-baselined away.

Every timing is normalised against a correctness render of the same config
first, because a performance result is only valid if its correctness checks
pass.

Exit codes: 0 ok, 1 fail, 2 skipped (CTest SKIP_RETURN_CODE 2). A failure is
never masked by a later skip (standard §5).

Usage:
    gate.py --renderer <bin> [--baseline bench/baseline.json]
            [--update --reason TEXT]
"""
import argparse
import csv
import io
import json
import math
import os
import subprocess
import sys
import tempfile
import time

_HERE = os.path.dirname(os.path.abspath(__file__))
_TESTS_GPU = os.path.abspath(os.path.join(_HERE, os.pardir, "tests", "gpu"))
for _path in (_HERE, _TESTS_GPU):
    if _path not in sys.path:
        sys.path.insert(0, _path)

import no_nan_check  # noqa: E402  (correctness normalisation before timing)
import reference  # noqa: E402

SKIP = 2
DEFAULT_BASELINE = os.path.join(_HERE, "baseline.json")
DEFAULT_TOLERANCE = 0.10

# Policy defaults, used only when validating a baseline that was seeded
# without them. Once bench/baseline.json is committed it is authoritative:
# re-recording measurements must never silently redefine the band or budgets.
DEFAULT_FRAMES = 20
DEFAULT_WARMUP = 5

_FLOAT_FIELDS = (
    "median_ms", "p95_ms", "mean_ms", "min_ms", "fps",
    "steps_per_ray", "stats_median_ms", "stats_overhead_pct",
)
_INT_FIELDS = (
    "width", "height", "max_steps", "jet_enabled", "disk_enabled",
    "frames", "warmup",
)
# Axes of the configuration matrix: if any of these drift between the recorded
# baseline and the run being compared, the two numbers describe different work
# and the comparison is meaningless.
_CONFIG_FIELDS = ("width", "height", "max_steps", "jet_enabled", "disk_enabled")

CSV_HEADER = (
    "median_ms,p95_ms,mean_ms,min_ms,fps,renderer,steps_per_ray,"
    "stats_median_ms,stats_overhead_pct,width,height,max_steps,"
    "jet_enabled,disk_enabled,frames,warmup"
)


# ════════════════════════════════════════════════════════════════════════════
# Pure decision logic — no I/O, no subprocesses. Covered by bench/test_gate.py.
# ════════════════════════════════════════════════════════════════════════════

def csv_header():
    """The header line `--bench` publishes (pinned by the unit test)."""
    return CSV_HEADER


def parse_bench_csv(text):
    """Extract the `--bench` row from renderer output.

    Returns a dict of typed fields, or None when there is no complete,
    well-formed row (missing header, truncated row, non-numeric field). A row
    short of its header would parse into the wrong columns, which is worse
    than no row at all, so it is refused rather than repaired.
    """
    lines = text.splitlines()
    header_index = None
    for index, line in enumerate(lines):
        if line.strip() == CSV_HEADER:
            header_index = index
            break
    if header_index is None or header_index + 1 >= len(lines):
        return None

    row_line = lines[header_index + 1].strip()
    if not row_line:
        return None
    try:
        values = next(csv.reader(io.StringIO(row_line)))
    except StopIteration:
        return None

    names = CSV_HEADER.split(",")
    if len(values) != len(names):
        return None

    parsed = {}
    for name, raw in zip(names, values):
        if name == "renderer":
            parsed["renderer"] = raw
            continue
        try:
            if name in _INT_FIELDS:
                parsed[name] = int(raw)
            else:
                parsed[name] = float(raw)
        except ValueError:
            return None
    return parsed


def evaluate(actual, baseline, tolerance=DEFAULT_TOLERANCE):
    """Compare one measurement with its baseline.

    Returns (verdict, ratio) where verdict is one of:
      "ok"          — inside ±tolerance
      "regression"  — slower than the band allows
      "improvement" — faster than the band allows: flagged, not celebrated,
                      because a suspicious improvement is as likely to be a
                      broken measurement as a genuine win
      "invalid"     — the measurement cannot be trusted at all

    Raises ValueError when the baseline itself is unusable, so a malformed
    baseline fails loudly instead of dividing by zero.
    """
    if baseline is None or not math.isfinite(baseline) or baseline <= 0.0:
        raise ValueError("baseline median must be a positive finite number")
    if actual is None or not math.isfinite(actual) or actual <= 0.0:
        return "invalid", float("nan")

    ratio = actual / baseline
    if ratio > 1.0 + tolerance:
        return "regression", ratio
    if ratio < 1.0 - tolerance:
        return "improvement", ratio
    return "ok", ratio


def jet_budget_findings(marginal_pct, budget_pct, tolerance=DEFAULT_TOLERANCE,
                        on_name="", off_name=""):
    """The jet pair's two rules: over budget, or suspiciously fast.

    Both are symmetric around the reference: `tolerance` is the same ±10 % band
    a single case is judged by, because jet emission only *adds* work — the jet
    being far faster is a measurement artefact, not a win to re-baseline.
    Returns [] when nothing applies.
    """
    if marginal_pct is None:
        return []
    findings = []
    if budget_pct and marginal_pct > budget_pct:
        findings.append(
            "jet: marginal cost {:.1f}% exceeds the ≤ {:.0f}% budget "
            "({} on vs {} off)".format(
                marginal_pct, budget_pct, on_name, off_name))
    elif marginal_pct < -tolerance * 100.0:
        findings.append(
            "jet: suspicious improvement — the jet makes the frame {:.1f}% "
            "faster ({} vs {}); emitting jet light cannot reduce the work, so "
            "confirm the measurement before re-baselining".format(
                -marginal_pct, on_name, off_name))
    return findings


def jet_marginal_pct(jet_ms, base_ms):
    """Marginal cost of enabling the jet, in percent of the disk-only frame."""
    if base_ms is None or not math.isfinite(base_ms) or base_ms <= 0.0:
        return float("inf")
    if jet_ms is None or not math.isfinite(jet_ms) or jet_ms <= 0.0:
        return float("inf")
    return (jet_ms - base_ms) / base_ms * 100.0


def measurement_findings(name, actual):
    """Validity of one measurement itself, independent of any baseline.

    A row that cannot be trusted is an error in its own right: it must never
    be compared against a baseline, and it must never be recorded as one.
    """
    findings = []
    median = actual.get("median_ms")
    if not isinstance(median, (int, float)) or not math.isfinite(median) or median <= 0.0:
        findings.append(
            "{}: no valid samples (median={!r}); the timing is unusable, so "
            "nothing is published for it".format(name, median))
    # The derived diagnostic is part of the ticket: a row without it is an
    # incomplete measurement, not a zero to be compared.
    steps = actual.get("steps_per_ray")
    if not isinstance(steps, (int, float)) or not math.isfinite(steps) or steps <= 0.0:
        findings.append(
            "{}: missing derived diagnostic — steps_per_ray is {}; the RK4 "
            "stats pass did not run".format(name, steps))
    return findings


def comparative_findings(name, actual, baseline, tolerance=DEFAULT_TOLERANCE,
                         budgets=None):
    """How one measurement compares with its recorded baseline.

    Raises ValueError when the baseline entry cannot be compared against —
    a baseline that claims a measurement but records no usable median is a
    defect in the committed file, not something to quietly pass.
    """
    if baseline is None:
        raise ValueError("no baseline entry for case '{}'".format(name))
    baseline_median = baseline.get("median_ms")
    if not isinstance(baseline_median, (int, float)) or not math.isfinite(
            baseline_median) or baseline_median <= 0.0:
        raise ValueError(
            "baseline case '{}' has no usable median_ms ({!r})".format(
                name, baseline_median))

    findings = []
    verdict, ratio = evaluate(actual.get("median_ms"), baseline_median,
                              tolerance)

    if verdict == "regression":
        findings.append(
            "{}: regression — median {:.3f} ms vs baseline {:.3f} ms "
            "(+{:.1f}%, band is ±{:.0f}%)".format(
                name, actual["median_ms"], baseline_median,
                (ratio - 1.0) * 100.0, tolerance * 100.0))
    elif verdict == "improvement":
        findings.append(
            "{}: suspicious improvement — median {:.3f} ms vs baseline {:.3f} "
            "ms ({:.1f}%, band is ±{:.0f}%); confirm it is a real win and not "
            "a broken measurement before re-baselining".format(
                name, actual["median_ms"], baseline_median,
                (ratio - 1.0) * 100.0, tolerance * 100.0))

    # The baseline describes a specific workload.
    for field in _CONFIG_FIELDS:
        if field in baseline and actual.get(field) != baseline.get(field):
            findings.append(
                "{}: config drift — {} is {} but the baseline recorded {}; "
                "the two numbers are not comparable".format(
                    name, field, actual.get(field), baseline.get(field)))

    # Reference-GPU budgets apply only at the resolution they were recorded
    # for; other matrix points are reported, not gated, by them.
    if budgets:
        budget_ms = budgets.get("full_frame_ms")
        if (budget_ms and actual.get("width") == budgets.get("budget_width")
                and actual.get("height") == budgets.get("budget_height")):
            median = actual.get("median_ms")
            if isinstance(median, (int, float)) and median > budget_ms:
                findings.append(
                    "{}: exceeds the reference-GPU full-frame budget — "
                    "{:.3f} ms vs ≤ {:.1f} ms at {}x{}".format(
                        name, median, budget_ms,
                        budgets.get("budget_width"),
                        budgets.get("budget_height")))
    return findings


def split_findings(name, actual, baseline, tolerance=DEFAULT_TOLERANCE,
                   budgets=None, with_comparisons=True):
    """(errors, comparisons) for one case — the gate's two severities.

    Errors are defects in the measurement itself and block every mode, including
    `--update`; comparisons judge the timing against the baseline and are
    informational while re-recording. This is the single source both the report
    and the exit code are built from.
    """
    errors = measurement_findings(name, actual)
    comparisons = []
    if with_comparisons:
        comparisons = comparative_findings(name, actual, baseline, tolerance,
                                           budgets)
    return errors, comparisons


def evaluate_case(name, actual, baseline, tolerance=DEFAULT_TOLERANCE,
                  budgets=None):
    """All findings for one case; an empty list means the case passes.

    Every finding is a complete sentence: the report prints them verbatim, so
    a finding has to be understandable on its own.
    """
    errors, comparisons = split_findings(name, actual, baseline, tolerance,
                                         budgets)
    return errors + comparisons


def protocol_drift(actual_frames, actual_warmup, baseline_frames,
                   baseline_warmup):
    """A message when the sampling protocol differs from the recorded one.

    Medians taken over different sample counts — or after a different warm-up —
    are not comparable: re-running the same code with `--frames 3 --warmup 1`
    moves the median by more than the band does, because the GPU has not settled.
    Returns None when the protocols match.
    """
    if actual_frames == baseline_frames and actual_warmup == baseline_warmup:
        return None
    return ("protocol drift — measured warmup={} frames={} but the baseline "
            "recorded warmup={} frames={}; medians from different protocols are "
            "not comparable".format(actual_warmup, actual_frames, baseline_warmup,
                                    baseline_frames))


def environment_finding(actual_renderer, baseline_renderer):
    """A message when the run's GPU is not the one the baseline recorded.

    A timing difference measured on different hardware says nothing about the
    code — it is an incomparable environment, not a finding (standard §4: pin
    or record the environment). Returns None when the two match, or when
    either side is unknown.
    """
    if not actual_renderer or not baseline_renderer:
        return None
    if actual_renderer == baseline_renderer:
        return None
    return ("GL_RENDERER {!r} does not match the baseline's {!r}; timings are "
            "not comparable — re-record the baseline if this GPU is the new "
            "reference".format(actual_renderer, baseline_renderer))


def exit_code(findings, skipped_reason):
    """Fail is never masked by a later skip (standard §5)."""
    if findings:
        return 1
    if skipped_reason:
        return 2
    return 0


def validate_baseline(baseline, require_measurements=True):
    """Return an error string, or None when the baseline is usable.

    A malformed baseline is a hard failure: it is the contract the whole gate
    rests on, it is read from a committed file, and "cannot validate" has to be
    louder than any individual measurement. `require_measurements=False` is for
    the seeding pass of `--update`, where cases exist but no number has been
    recorded yet.
    """
    if not isinstance(baseline, dict):
        return "baseline is not a JSON object"
    for key in ("schema", "environment", "tolerance", "budgets", "cases"):
        if key not in baseline:
            return "baseline is missing '{}'".format(key)
    if baseline["schema"] != 1:
        return "unsupported baseline schema {!r} (expected 1)".format(
            baseline["schema"])
    tolerance = baseline["tolerance"]
    if not isinstance(tolerance, (int, float)) or not 0 < tolerance <= 0.10:
        return ("baseline tolerance {!r} is outside (0, 0.10]; a wider band "
                "would gate nothing".format(tolerance))
    if not isinstance(baseline["budgets"], dict):
        return "baseline 'budgets' must be an object"
    cases = baseline["cases"]
    if not isinstance(cases, list) or not cases:
        return "baseline 'cases' must be a non-empty list"
    seen = set()
    for case in cases:
        if not isinstance(case, dict):
            return "baseline has a case that is not an object"
        name = case.get("name")
        if not name:
            return "baseline has a case without a name"
        if name in seen:
            return "baseline has duplicate case '{}'".format(name)
        seen.add(name)
        if not case.get("config"):
            return "baseline case '{}' has no config".format(name)
        if require_measurements:
            median = case.get("median_ms")
            if not isinstance(median, (int, float)) or median <= 0:
                return ("baseline case '{}' has no usable median_ms "
                        "({!r})".format(name, median))
            for field in _CONFIG_FIELDS:
                if field not in case:
                    return "baseline case '{}' is missing '{}'".format(
                        name, field)
    environment = baseline["environment"]
    if not isinstance(environment, dict) or not environment.get("gl_renderer"):
        if require_measurements:
            return "baseline 'environment.gl_renderer' is required"
    return None


# ════════════════════════════════════════════════════════════════════════════
# Runner
# ════════════════════════════════════════════════════════════════════════════

def load_baseline(path):
    with open(path, "r") as handle:
        return json.load(handle)


def last_line(log):
    lines = log.strip().splitlines()
    return lines[-1] if lines else ""


def run_bench(renderer, config, warmup, frames):
    """Run `--bench <config>`; return (row, log, exit_code). row is None."""
    proc = subprocess.run(
        [renderer, "--bench", config,
         "--warmup", str(warmup), "--frames", str(frames)],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=dict(os.environ))
    log = proc.stdout.decode("utf-8", "replace")
    if proc.returncode != 0:
        return None, log, proc.returncode
    return parse_bench_csv(log), log, 0


def check_correctness(renderer, config, workdir, tag):
    """Normalise one timing against a rendered frame of the same config.

    Returns (findings, code): an empty findings list means the frame is
    finite, non-negative, inside the tone-mapped ceiling, and not blank. This
    is the standard's "every performance result is valid only if the
    corresponding correctness checks pass", applied before any number is
    published — and therefore also before any number is recorded.
    """
    out_path = os.path.join(workdir, "correctness_{}.f32".format(tag))
    code, log = reference.render(renderer, config, out_path)
    if code == SKIP:
        return (["{}: renderer could not create a usable GPU context for the "
                 "correctness pass".format(tag)], SKIP)
    if code != 0:
        return (["{}: correctness render failed (exit {}): {}".format(
            tag, code, last_line(log))], 1)
    defects, _shape = no_nan_check.check_buffer(out_path)
    return (["{}: correctness — {}".format(tag, d) for d in defects],
            0 if not defects else 1)


def format_report(rows, findings, skipped_reason):
    lines = []
    lines.append("{:<16} {:>9} {:>9} {:>9} {:>10} {:>7}".format(
        "case", "median", "p95", "ratio", "steps/ray", "verdict"))
    for row in rows:
        lines.append("{:<16} {:>9.3f} {:>9.3f} {:>9.4f} {:>10.1f} {:>7}".format(
            row["name"], row["actual"]["median_ms"], row["actual"]["p95_ms"],
            row["ratio"], row["actual"]["steps_per_ray"], row["verdict"]))
    if rows:
        ratios = [r["ratio"] for r in rows if math.isfinite(r["ratio"])]
        if ratios:
            lines.append("{:<16} worst {:.1%} / best {:.1%} against baseline"
                         .format("", max(ratios), min(ratios)))
    for finding in findings:
        lines.append("FAIL: " + finding)
    if skipped_reason:
        lines.append("SKIP: " + skipped_reason)
    return "\n".join(lines)


def build_baseline(previous, rows, renderer_name, reason, frames, warmup):
    """New baseline content: fresh measurements, everything else carried over.

    The whole point of the split between policy and measurement is that the
    command which records a number cannot also redefine the band or the budgets
    that number is judged by — so every committed key survives an update
    verbatim, and only the measured fields are replaced.
    """
    baseline = dict(previous)
    environment = dict(previous.get("environment", {}) or {})
    if renderer_name:
        environment["gl_renderer"] = renderer_name
    baseline["environment"] = environment
    baseline["frames"] = frames
    baseline["warmup"] = warmup
    baseline["cases"] = []
    baseline["last_update"] = {
        "reason": reason,
        "utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    }
    for row in rows:
        actual = row["actual"]
        baseline["cases"].append({
            "name": row["name"],
            "config": row["config"],
            "median_ms": actual["median_ms"],
            "p95_ms": actual["p95_ms"],
            "steps_per_ray": actual["steps_per_ray"],
            "stats_overhead_pct": actual["stats_overhead_pct"],
            "width": actual["width"],
            "height": actual["height"],
            "max_steps": actual["max_steps"],
            "jet_enabled": actual["jet_enabled"],
            "disk_enabled": actual["disk_enabled"],
        })
    return baseline


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--renderer", required=True)
    parser.add_argument("--baseline", default=DEFAULT_BASELINE)
    parser.add_argument("--tolerance", type=float, default=None,
                        help="override the band (default: the committed "
                             "baseline's)")
    parser.add_argument("--warmup", type=int, default=None)
    parser.add_argument("--frames", type=int, default=None)
    parser.add_argument("--workdir", default=None)
    parser.add_argument("--update", action="store_true",
                        help="re-record the baseline from this run")
    parser.add_argument("--reason", default=None,
                        help="required with --update; recorded in the JSON")
    parser.add_argument("--report", default=None,
                        help="write the machine-readable report here")
    args = parser.parse_args(argv)

    if args.update and not args.reason:
        print("gate: --update requires --reason (recorded in the baseline)",
              file=sys.stderr)
        return 1
    if not os.path.exists(args.baseline):
        print("gate: no baseline at {}; it carries the reviewed policy "
              "(tolerance, budgets) and the configuration matrix, so seed it "
              "before running".format(args.baseline), file=sys.stderr)
        return 1

    try:
        baseline = load_baseline(args.baseline)
    except (OSError, ValueError) as exc:
        print("gate: cannot read baseline {}: {}".format(args.baseline, exc),
              file=sys.stderr)
        return 1

    error = validate_baseline(baseline, require_measurements=not args.update)
    if error:
        print("gate: invalid baseline: {}".format(error), file=sys.stderr)
        return 1

    tolerance = (args.tolerance if args.tolerance is not None
                 else baseline["tolerance"])
    if not 0 < tolerance <= 0.10:
        print("gate: tolerance must be within (0, 0.10]; got {}".format(
            tolerance), file=sys.stderr)
        return 1

    frames = (args.frames if args.frames is not None
              else baseline.get("frames", DEFAULT_FRAMES))
    warmup = (args.warmup if args.warmup is not None
              else baseline.get("warmup", DEFAULT_WARMUP))
    budgets = baseline["budgets"]
    workdir = args.workdir or tempfile.mkdtemp(prefix="bench_gate_")
    os.makedirs(workdir, exist_ok=True)  # CTest may point at a fresh dir

    errors = []        # block every mode: the measurement is not publishable
    comparisons = []   # block the gate; informational while re-recording
    rows = []
    skip_reason = None
    renderer_name = None

    for case in baseline["cases"]:
        name, config = case["name"], case["config"]

        correctness, code = check_correctness(args.renderer, config, workdir,
                                              name)
        if code == SKIP:
            # An absent GPU is a skip, not a failure (ticket 0010): nothing was
            # measured, so nothing can be claimed — least of all a pass. A
            # failure found earlier still wins, via exit_code().
            skip_reason = skip_reason or (
                "renderer reported no usable GPU context ({})".format(
                    correctness[0] if correctness else ""))
            continue
        errors.extend(correctness)
        if code != 0:
            continue

        actual, log, code = run_bench(args.renderer, config, warmup, frames)
        if code == SKIP:
            skip_reason = skip_reason or "renderer reported no usable GPU"
            continue
        if code != 0:
            errors.append("{}: --bench failed (exit {}): {}".format(
                name, code, last_line(log)))
            continue
        if actual is None:
            errors.append(
                "{}: no --bench row in the renderer output".format(name))
            continue

        if renderer_name is None:
            renderer_name = actual.get("renderer")
        elif actual.get("renderer") != renderer_name:
            errors.append("{}: renderer changed mid-run ({!r} vs {!r})".format(
                name, actual.get("renderer"), renderer_name))

        case_findings, case_comparisons = split_findings(
            name, actual, case, tolerance, budgets,
            with_comparisons=not args.update)
        errors.extend(case_findings)
        comparisons.extend(case_comparisons)

        baseline_median = case.get("median_ms")
        rows.append({
            "name": name,
            "config": config,
            "actual": actual,
            "ratio": (actual["median_ms"] / baseline_median
                      if isinstance(baseline_median, (int, float))
                      and baseline_median > 0 else float("nan")),
            # Verdicts are settled once the whole run is known: on hardware
            # other than the baseline's there is nothing to judge, only an
            # environment to report.
            "case_findings": case_findings,
            "case_comparisons": case_comparisons,
        })

    # Same code, different sampling protocol → different median (measured: a
    # 1-warmup/3-frame run reads ~11 % high on the 1024² case).
    if rows and not args.update:
        drift = protocol_drift(rows[0]["actual"].get("frames"),
                               rows[0]["actual"].get("warmup"),
                               baseline.get("frames"), baseline.get("warmup"))
        if drift:
            comparisons.append(drift)

    # Derived diagnostic: marginal cost of the jet, from the same timed run.
    jet_pair = budgets.get("jet_pair") if isinstance(budgets, dict) else None
    jet_marginal = None
    if jet_pair and len(jet_pair) == 2:
        by_name = {row["name"]: row for row in rows}
        if jet_pair[0] in by_name and jet_pair[1] in by_name:
            jet_marginal = jet_marginal_pct(
                by_name[jet_pair[1]]["actual"]["median_ms"],
                by_name[jet_pair[0]]["actual"]["median_ms"])
            comparisons.extend(
                jet_budget_findings(jet_marginal,
                                    budgets.get("jet_marginal_pct"),
                                    tolerance, jet_pair[1], jet_pair[0]))

    # The baseline belongs to one GPU. On any other hardware a timing
    # difference is not a finding but an incomparable environment, so the run
    # is measured and reported and then skipped — never passed, never failed
    # on evidence the hardware invalidates. Correctness failures survive this:
    # they do not depend on how fast the machine is, and fail is never masked
    # by a skip.
    env_mismatch = environment_finding(
        renderer_name, baseline["environment"].get("gl_renderer"))
    if env_mismatch:
        comparisons = []
        for row in rows:
            row["case_comparisons"] = []
    skipping = bool(env_mismatch) and not errors and not args.update
    if skipping:
        skip_reason = skip_reason or env_mismatch

    for row in rows:
        row["verdict"] = ("fail" if (row["case_findings"]
                                     or row["case_comparisons"])
                          else "skip" if skipping
                          else "ok")
        row["findings"] = row["case_findings"] + row["case_comparisons"]

    if args.update:
        if errors:
            print(format_report(rows, errors + comparisons, skip_reason))
            print("gate: refusing to record a baseline while measurements "
                  "are not publishable", file=sys.stderr)
            return 1
        new_baseline = build_baseline(baseline, rows, renderer_name,
                                      args.reason, frames, warmup)
        with open(args.baseline, "w") as handle:
            json.dump(new_baseline, handle, indent=2)
            handle.write("\n")
        print(format_report(rows, [], None))
        print("gate: baseline updated ({}, {} cases) — reason: {}".format(
            args.baseline, len(rows), args.reason))
        if env_mismatch:
            # Re-recording on different hardware is deliberate here (that is
            # what --reason is for), but the budgets were recorded elsewhere
            # and are policy: this command cannot and does not redefine them.
            print("gate: note — recorded on {!r}; the previous baseline was "
                  "recorded on {!r}. Budgets carried over unchanged; they "
                  "still describe the previous GPU.".format(
                      renderer_name, baseline["environment"].get("gl_renderer")),
                  file=sys.stderr)
        return 0

    findings = errors + comparisons
    print(format_report(rows, findings, skip_reason))
    if jet_marginal is not None:
        print("jet: marginal cost {:.1f}% (budget ≤ {:.0f}%)".format(
            jet_marginal, budgets.get("jet_marginal_pct")))

    if args.report:
        with open(args.report, "w") as handle:
            json.dump({
                "gl_renderer": renderer_name,
                "tolerance": tolerance,
                "jet_marginal_pct": jet_marginal,
                "cases": rows,
                "findings": findings,
                "skipped": skip_reason,
            }, handle, indent=2)
            handle.write("\n")

    return exit_code(findings, skip_reason)


if __name__ == "__main__":
    sys.exit(main())
