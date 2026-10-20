#!/usr/bin/env python3
"""Self-verification of the golden comparator (ticket 0011).

The harness verifies itself on unambiguous cases (spec 0002 §"Testing"):
identical buffers, a single known perturbation, threshold fractions, and
shape-mismatch handling. No GPU required.
"""
import array
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import compare  # noqa: E402

FAILURES = []


def check(cond, what):
    if not cond:
        FAILURES.append(what)
        print("  FAIL {}".format(what), file=sys.stderr)


def approx(a, b, tol=1e-6):
    return abs(a - b) <= tol


def write_case(path, values, width, height):
    arr = array.array("f", values)
    with open(path, "wb") as handle:
        arr.tofile(handle)
    with open(path + ".meta", "w") as handle:
        handle.write("width={}\nheight={}\nchannels=4\n".format(width, height))


def main():
    # ── Identical buffers: everything is zero ────────────────────────────────
    a = array.array("f", [0.0, 1.0, 0.5, 0.25])
    r = compare.compare(a, a, threshold=1e-3)
    check(r["max_abs"] == 0.0, "identical: max_abs == 0")
    check(r["mean_abs"] == 0.0, "identical: mean_abs == 0")
    check(r["count_above"] == 0 and r["frac_above"] == 0.0, "identical: no element above threshold")

    # ── One known perturbation ───────────────────────────────────────────────
    b = array.array("f", [0.0, 1.1, 0.5, 0.25])  # index 1 differs by 0.1
    r = compare.compare(a, b, threshold=0.05)
    check(approx(r["max_abs"], 0.1, 1e-6), "perturb: max_abs ~ 0.1")
    check(approx(r["mean_abs"], 0.1 / 4, 1e-6), "perturb: mean_abs ~ 0.025")
    check(r["count_above"] == 1, "perturb: one element above 0.05")
    check(approx(r["frac_above"], 0.25, 1e-6), "perturb: frac_above == 0.25")
    check(r["n"] == 4, "perturb: n == 4")

    # Threshold just below the perturbation still counts exactly one.
    r = compare.compare(a, b, threshold=0.099)
    check(r["count_above"] == 1, "perturb: still one above 0.099")

    # ── Non-finite handling fails loudly (never silently ignored) ────────────
    c = array.array("f", [0.0, float("nan"), 0.5, 0.25])
    r = compare.compare(a, c, threshold=1e-3)
    check(r["max_abs"] == float("inf"), "nan: max_abs is inf")

    # ── Length mismatch is an error ──────────────────────────────────────────
    try:
        compare.compare(array.array("f", [1.0]), array.array("f", [1.0, 2.0]), 1e-3)
        check(False, "length mismatch raises")
    except ValueError:
        check(True, "length mismatch raises")

    # ── File + sidecar path ──────────────────────────────────────────────────
    with tempfile.TemporaryDirectory() as tmp:
        pa = os.path.join(tmp, "a.f32")
        pb = os.path.join(tmp, "b.f32")
        write_case(pa, [0.0, 1.0, 0.5, 0.25], width=1, height=1)
        write_case(pb, [0.0, 1.0, 0.5, 0.26], width=1, height=1)
        r = compare.compare_files(pa, pb, threshold=1e-3)
        check(approx(r["max_abs"], 0.01, 1e-6), "files: max_abs ~ 0.01")
        check(r["shape"] == (1, 1, 4), "files: shape parsed from sidecar")

        # A wrong declared shape must fail loudly, not compare mismatched bytes.
        write_case(pb, [0.0, 1.0, 0.5, 0.26], width=2, height=1)
        try:
            compare.compare_files(pa, pb, threshold=1e-3)
            check(False, "sidecar shape mismatch raises")
        except ValueError:
            check(True, "sidecar shape mismatch raises")

    if FAILURES:
        print("FAIL: {} check(s) failed".format(len(FAILURES)), file=sys.stderr)
        return 1
    print("PASS: comparator self-test")
    return 0


if __name__ == "__main__":
    sys.exit(main())
