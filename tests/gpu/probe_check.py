#!/usr/bin/env python3
"""GPU↔CPU probe cross-validation (ticket 0012, seam S3).

Runs the two halves of the shared 48-case probe table from
`include/core/probe_cases.h` and compares them:

* `probe_reference --out <file>` evaluates the table in double precision in
  pure C++ (no GL, no GLFW) — the authority for "what the kernel means".
* `<renderer> --probe <file>` evaluates the *same* records through
  `shaders/probe.comp`, i.e. through the extracted `physics.glsl` kernels,
  as float32 GPU work items.

Agreement means the GLSL kernels are the same functions as the CPU reference,
which is the only claim a source-level extraction can make: the compiler, the
GPU driver and float32 rounding are all outside our control.

Exit codes: 0 ok, 1 fail, 2 skipped (CTest SKIP_RETURN_CODE 2). The renderer
exits 2 when it cannot open a GL 4.3 compute context; that is reported as a
skip, never as a pass.

Usage:
    probe_check.py --renderer <bin> --probe-ref <bin> --workdir <dir>
                   [--rtol RTOL] [--tolerance TOL]
"""
import argparse
import os
import subprocess
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import compare  # noqa: E402
import reference  # noqa: E402

SKIP = 2

# Justification (standards §3 "Justify every tolerance"):
#   * `tolerance` (absolute) — the compared magnitudes span ~9 orders of
#     magnitude (2e-4 to ~1.5e2 for beamed intensities), so the absolute term
#     only has to cover float32 rounding of the *large* values: 1e-4 is ~0.6
#     ulp at 232, the largest single absolute deviation measured.
#   * `rtol` (relative) — float32 versus double disagrees by ~7 significant
#     digits; 1e-5 is ~85 ulp, i.e. two orders of margin over the measured
#     worst relative deviation (2.4e-5 on a 2.4e-4 value, where the absolute
#     term already dominates the budget).
# Measured agreement across all 48 cases: max_abs 1.07e-4, max_rel 2.4e-5.
DEFAULT_RTOL = 1e-5
DEFAULT_TOLERANCE = 1e-4


def run_cpu(probe_ref, out_path):
    """Evaluate the probe table on the CPU. Core-only: never skips."""
    if os.path.exists(out_path):
        os.remove(out_path)
    proc = subprocess.run(
        [probe_ref, "--out", out_path],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if proc.returncode != 0:
        return proc.returncode, "probe_reference exited {}: {}".format(
            proc.returncode, (proc.stdout + proc.stderr).strip())
    if not os.path.exists(out_path):
        return 1, "probe_reference wrote no output at {}".format(out_path)
    return 0, proc.stdout.strip()


def run_gpu(renderer, out_path):
    """Evaluate the probe table on the GPU. Returns (code, log, renderer)."""
    if os.path.exists(out_path):
        os.remove(out_path)
    proc = subprocess.run(
        [renderer, "--probe", out_path],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    log = proc.stdout + proc.stderr
    if proc.returncode == SKIP:
        return SKIP, log.strip(), None
    if proc.returncode != 0:
        return 1, "renderer exited {}: {}".format(
            proc.returncode, log.strip()), None
    if not os.path.exists(out_path):
        return 1, "renderer wrote no output at {}".format(out_path), None
    return 0, log.strip(), reference.gl_renderer_from_log(log)


def validate_sidecar(path, expect_source):
    """Read and check the probe sidecar; returns (case_count, gl_renderer)."""
    try:
        meta = compare.read_meta(path + ".meta")
    except OSError as exc:
        raise ValueError("{}: no sidecar: {}".format(path, exc))
    for key in ("width", "height", "channels", "case_count", "dtype",
                "layout"):
        if key not in meta:
            raise ValueError("{}: sidecar missing {!r}".format(path, key))
    if meta["dtype"] != "float32" or meta["layout"] != "probe-vec4":
        raise ValueError("{}: unexpected dtype/layout {} / {}".format(
            path, meta["dtype"], meta["layout"]))
    if meta.get("source") != expect_source:
        raise ValueError("{}: sidecar source is {!r}, expected {!r}".format(
            path, meta.get("source"), expect_source))
    try:
        case_count = int(meta["case_count"])
    except ValueError:
        raise ValueError("{}: bad case_count".format(path))
    if case_count <= 0:
        raise ValueError("{}: case_count is {}".format(path, case_count))
    return case_count, meta


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--renderer", required=True)
    parser.add_argument("--probe-ref", required=True)
    parser.add_argument("--workdir", required=True)
    parser.add_argument("--rtol", type=float, default=DEFAULT_RTOL)
    parser.add_argument("--tolerance", type=float, default=DEFAULT_TOLERANCE)
    args = parser.parse_args(argv)

    os.makedirs(args.workdir, exist_ok=True)
    cpu_path = os.path.join(args.workdir, "probe_cpu.f32")
    gpu_path = os.path.join(args.workdir, "probe_gpu.f32")

    code, log = run_cpu(args.probe_ref, cpu_path)
    if code != 0:
        print("FAIL: {}".format(log), file=sys.stderr)
        return 1

    code, log, gl_renderer = run_gpu(args.renderer, gpu_path)
    if code == SKIP:
        print("SKIPPED: no GL 4.3 compute context")
        if log:
            print(log)
        return SKIP
    if code != 0:
        print("FAIL: {}".format(log), file=sys.stderr)
        return 1

    try:
        cpu_cases, _ = validate_sidecar(cpu_path, "cpu")
        gpu_cases, _ = validate_sidecar(gpu_path, "gpu")
    except ValueError as exc:
        print("FAIL: {}".format(exc), file=sys.stderr)
        return 1
    if cpu_cases != gpu_cases:
        print("FAIL: case count mismatch: cpu {} vs gpu {}".format(
            cpu_cases, gpu_cases), file=sys.stderr)
        return 1

    try:
        report = compare.compare_files(
            cpu_path, gpu_path,
            threshold=args.tolerance, rtol=args.rtol,
            tolerance=args.tolerance)
    except (OSError, ValueError) as exc:
        print("FAIL: {}".format(exc), file=sys.stderr)
        return 1

    where = " (GL renderer: {})".format(gl_renderer) if gl_renderer else ""
    print("{} cases, budget |cpu-gpu| <= {} + {}*|gpu|{}".format(
        report["n"], args.tolerance, args.rtol, where))
    print(compare.format_report(report))
    if report["rel_violations"] == 0:
        print("PASS: GPU physics kernels agree with the CPU reference")
        return 0
    print("FAIL: {} of {} element(s) outside the budget".format(
        report["rel_violations"], report["n"]), file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
