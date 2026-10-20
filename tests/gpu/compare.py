#!/usr/bin/env python3
"""Golden-comparison tool for the raw float32 reference buffers (ticket 0011).

Compares two raw little-endian float32 RGBA buffers and reports the maximum
absolute difference, the mean absolute difference, and the fraction of
elements above a threshold — the minimum required by
docs/standards/testing-and-performance.md §"Reference (golden) format".

The comparator runs on the raw buffers, never on the quantised PPM. Shape is
read from the `<file>.meta` sidecar and a mismatch fails loudly rather than
comparing the wrong bytes. Stdlib only.

An element passes the combined budget when

    |a - b| <= tolerance + rtol * |b|

`tolerance` is the absolute half, `rtol` the relative half. `rtol` defaults to
0.0, so the comparison degenerates to the purely absolute `max_abs <= tolerance`
rule that the golden-regression harness (ticket 0011) depends on — adding the
relative term cannot loosen any existing call site.

CLI:
    compare.py <a.f32> <b.f32> [--threshold T] [--tolerance TOL] [--rtol RTOL]
Exit 0 when every element passes the combined budget, else 1.
"""
import array
import math
import os
import sys

# Justification (standards §3 "Justify every tolerance"): 1e-3 is ~1/4 of one
# 8-bit code value (1/255 = 3.9e-3), so a comparison that passes is already
# invisible in the quantised viewable form. Identical builds on the reference
# machine agree exactly (max_abs = 0); this is headroom for float32
# re-evaluation of transcendentals across runs, never for a visible change.
DEFAULT_THRESHOLD = 1e-3
DEFAULT_TOLERANCE = 1e-3
# Default 0.0 = purely absolute, byte-for-byte the pre-0012 rule. Callers that
# compare a float32 GPU result against a double-precision CPU reference opt in
# to a relative term, because the two disagree by ~7 significant digits and the
# compared magnitudes span ~9 orders (2e-4 up to ~150 for beamed intensities):
# no single absolute tolerance can be both tight and valid across that range.
DEFAULT_RTOL = 0.0


def read_meta(path):
    """Parse a simple `key=value` sidecar into a dict."""
    meta = {}
    with open(path) as handle:
        for line in handle:
            line = line.strip()
            if not line or "=" not in line:
                continue
            key, value = line.split("=", 1)
            meta[key.strip()] = value.strip()
    return meta


def load_f32(path):
    """Load a raw little-endian float32 buffer as an array('f')."""
    itemsize = array.array("f").itemsize
    arr = array.array("f")
    size = os.path.getsize(path)
    if size % itemsize != 0:
        raise ValueError("{}: size {} is not a multiple of {}".format(path, size, itemsize))
    with open(path, "rb") as handle:
        arr.fromfile(handle, size // itemsize)
    if sys.byteorder != "little":
        arr.byteswap()
    return arr


def shape_from_meta(meta, n_floats, path):
    try:
        width = int(meta["width"])
        height = int(meta["height"])
        channels = int(meta["channels"])
    except (KeyError, ValueError) as exc:
        raise ValueError("{}: sidecar missing/invalid shape key: {}".format(path, exc))
    expected = width * height * channels
    if expected != n_floats:
        raise ValueError(
            "{}: sidecar declares {}x{}x{} = {} floats, file has {}".format(
                path, width, height, channels, expected, n_floats))
    return (width, height, channels)


def compare(a, b, threshold=DEFAULT_THRESHOLD, rtol=DEFAULT_RTOL,
            tolerance=DEFAULT_TOLERANCE):
    """Compare two float sequences. Returns a report dict."""
    if len(a) != len(b):
        raise ValueError("length mismatch: {} vs {}".format(len(a), len(b)))
    n = len(a)
    if n == 0:
        raise ValueError("empty buffers")
    max_abs = 0.0
    sum_abs = 0.0
    count_above = 0
    rel_violations = 0
    for x, y in zip(a, b):
        if math.isfinite(x) and math.isfinite(y):
            d = abs(x - y)
        else:
            d = math.inf  # NaN / inf must never be silently equal
        sum_abs += d
        if d > max_abs:
            max_abs = d
        if d > threshold:
            count_above += 1
        # Reject non-finite inputs before the budget test: with rtol > 0 the
        # budget itself would be inf when y is inf, which would let two
        # identical infinities pass as "equal".
        if d == math.inf or not d <= tolerance + rtol * abs(y):
            rel_violations += 1
    return {
        "n": n,
        "max_abs": max_abs,
        "mean_abs": sum_abs / n,
        "count_above": count_above,
        "frac_above": count_above / n,
        "threshold": threshold,
        "tolerance": tolerance,
        "rtol": rtol,
        "rel_violations": rel_violations,
    }


def compare_files(path_a, path_b, threshold=DEFAULT_THRESHOLD,
                  rtol=DEFAULT_RTOL, tolerance=DEFAULT_TOLERANCE):
    """Compare two .f32 files with sidecar-validated shapes."""
    a = load_f32(path_a)
    b = load_f32(path_b)
    shape_a = shape_from_meta(read_meta(path_a + ".meta"), len(a), path_a)
    shape_b = shape_from_meta(read_meta(path_b + ".meta"), len(b), path_b)
    if shape_a != shape_b:
        raise ValueError("shape mismatch: {} vs {}".format(shape_a, shape_b))
    report = compare(a, b, threshold=threshold, rtol=rtol, tolerance=tolerance)
    report["shape"] = shape_a
    return report


def format_report(report):
    return ("max_abs={max_abs:.6g} mean_abs={mean_abs:.6g} "
            "frac_above({threshold:g})={frac_above:.6g} "
            "({count_above}/{n})").format(**report)


def _parse_args(argv):
    positional = []
    threshold = DEFAULT_THRESHOLD
    tolerance = DEFAULT_TOLERANCE
    rtol = DEFAULT_RTOL
    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg == "--threshold":
            threshold = float(argv[i + 1]); i += 2
        elif arg == "--tolerance":
            tolerance = float(argv[i + 1]); i += 2
        elif arg == "--rtol":
            rtol = float(argv[i + 1]); i += 2
        elif arg in ("-h", "--help"):
            print(__doc__.strip())
            sys.exit(0)
        else:
            positional.append(arg); i += 1
    return positional, threshold, tolerance, rtol


def main(argv):
    positional, threshold, tolerance, rtol = _parse_args(argv)
    if len(positional) != 2:
        print("usage: compare.py <a.f32> <b.f32> "
              "[--threshold T] [--tolerance TOL] [--rtol RTOL]", file=sys.stderr)
        return 2
    path_a, path_b = positional
    try:
        report = compare_files(path_a, path_b, threshold=threshold,
                               rtol=rtol, tolerance=tolerance)
    except (OSError, ValueError) as exc:
        print("FAIL: {}".format(exc), file=sys.stderr)
        return 1
    print(format_report(report))
    if report["rel_violations"] == 0:
        print("PASS: all {} element(s) within |a-b| <= {} + {}*|b|".format(
            report["n"], tolerance, rtol))
        return 0
    print("FAIL: {} element(s) outside |a-b| <= {} + {}*|b| "
          "(max_abs={})".format(report["rel_violations"], tolerance, rtol,
                                report["max_abs"]), file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
