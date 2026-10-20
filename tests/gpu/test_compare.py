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

    # ── Relative tolerance (ticket 0012: GPU float32 vs CPU double) ──────────
    # 1000.0 vs 1000.001: absolute error 1e-3, relative error ~1e-6.
    big = array.array("f", [1000.0])
    near = array.array("f", [1000.001])
    r = compare.compare(big, near, threshold=0.0, rtol=1e-5, tolerance=1e-4)
    check(r["rel_violations"] == 0, "rtol: 1e-3 abs / 1e-6 rel accepted")
    check(r["rtol"] == 1e-5, "rtol: reported")

    # The same error must be rejected when no relative budget is granted.
    r = compare.compare(big, near, threshold=0.0, rtol=0.0, tolerance=1e-4)
    check(r["rel_violations"] == 1, "rtol: rejected at rtol=0")
    # A pure absolute budget that already covers it must still accept it.
    r = compare.compare(big, near, threshold=0.0, rtol=0.0, tolerance=1e-2)
    check(r["rel_violations"] == 0, "rtol: absolute budget alone still works")

    # 1000.0 vs 1001.0: relative error 1e-3 — far outside a 1e-5 relative
    # budget, and an absolute 1e-4 must not rescue it either.
    far = array.array("f", [1001.0])
    r = compare.compare(big, far, threshold=0.0, rtol=1e-5, tolerance=1e-4)
    check(r["rel_violations"] == 1, "rtol: 1e-3 relative error rejected")

    # Relative tolerance must not open a hole for non-finite values: two
    # matching infinities have a limit of inf once rtol > 0, so the budget
    # test has to reject them explicitly.
    inf_a = array.array("f", [float("inf")])
    inf_b = array.array("f", [float("inf")])
    r = compare.compare(inf_a, inf_b, threshold=0.0, rtol=1e-5, tolerance=1e-4)
    check(r["rel_violations"] == 1, "rtol: matching infinities rejected")

    # Default is purely absolute — the golden rule of ticket 0011 is unchanged.
    r = compare.compare(array.array("f", [1.0]), array.array("f", [1.0]))
    check(r["rtol"] == 0.0, "rtol: default is 0.0")

    if FAILURES:
        print("FAIL: {} check(s) failed".format(len(FAILURES)), file=sys.stderr)
        return 1
    print("PASS: comparator self-test")
    return 0


if __name__ == "__main__":
    sys.exit(main())
