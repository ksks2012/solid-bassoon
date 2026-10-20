#!/usr/bin/env python3
"""Full-render numeric sanity: no NaN/Inf, no negatives, tone-mapped ceiling
(ticket 0012, seam S4).

The golden regression (ticket 0011) answers "does the frame still match the
reference?". It cannot answer "is the frame physically sane?" — a buffer full
of NaNs would only pass if the reference were equally broken, and a negative
radiance is exactly the kind of defect that shows up as black or flickering
pixels rather than as a tolerance breach.

This renders every scene in `<reference-dir>/scenes` and asserts, per scene:

  1. every float is finite (no NaN, no ±Inf);
  2. no float is negative — radiance and tone-mapped colour are non-negative;
  3. no float exceeds 1 + CEILING_SLACK — ADR-0001 contracts a tone-mapped
     ceiling of 1.0, so anything above it is HDR leaking past the tone map;
  4. the frame is not entirely zero (a blank render is not a pass).

Exit codes: 0 ok, 1 fail, 2 skipped (CTest SKIP_RETURN_CODE 2).

Usage:
    no_nan_check.py --renderer <bin> --reference-dir <dir> [--workdir <dir>]
"""
import argparse
import math
import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import compare  # noqa: E402
import reference  # noqa: E402

SKIP = 2

# Float32 rounding of a tone-mapped value that saturates the curve cannot
# exceed 1.0 by more than a few ulp; 1e-5 is ~85 ulp at 1.0, so a real leak
# (an untone-mapped HDR value of, say, 1.679 for a 2.0x boost) is caught many
# times over while driver rounding is not.
CEILING_SLACK = 1e-5


def check_buffer(path):
    """Return (defects, (width, height)) for one render buffer."""
    try:
        data = compare.load_f32(path)
        meta = compare.read_meta(path + ".meta")
        width, height, channels = compare.shape_from_meta(meta, len(data), path)
    except (OSError, ValueError) as exc:
        return ["cannot load buffer: {}".format(exc)], (0, 0)

    defects = []
    if channels != 4:
        defects.append("expected 4 channels, got {}".format(channels))
    if not data:
        defects.append("empty buffer")
        return defects, (width, height)

    non_finite = 0
    first_non_finite = None
    negative = 0
    first_negative = None
    above_ceiling = 0
    first_above = None
    zero_count = 0
    for index, value in enumerate(data):
        if not math.isfinite(value):
            non_finite += 1
            if first_non_finite is None:
                first_non_finite = (index, value)
            continue  # comparisons against NaN/Inf would be meaningless
        if value < 0.0:
            negative += 1
            if first_negative is None:
                first_negative = (index, value)
        elif value > 1.0 + CEILING_SLACK:
            above_ceiling += 1
            if first_above is None:
                first_above = (index, value)
        if value == 0.0:
            zero_count += 1

    if non_finite:
        defects.append("{} non-finite value(s), first at index {} = {}".format(
            non_finite, *first_non_finite))
    if negative:
        defects.append("{} negative value(s), first at index {} = {}".format(
            negative, *first_negative))
    if above_ceiling:
        defects.append("{} value(s) above 1.0+{}, first at index {} = {}".format(
            above_ceiling, CEILING_SLACK, *first_above))
    if zero_count == len(data):
        defects.append("all {} value(s) are zero (blank render)".format(len(data)))
    return defects, (width, height)


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--renderer", required=True)
    parser.add_argument("--reference-dir", required=True)
    parser.add_argument("--workdir",
                        default=os.path.join(os.getcwd(), "no_nan_work"))
    args = parser.parse_args(argv)

    scenes = reference.scene_names(args.reference_dir)
    if not scenes:
        print("FAIL: no scenes under {}".format(
            reference.scenes_dir(args.reference_dir)), file=sys.stderr)
        return 1
    os.makedirs(args.workdir, exist_ok=True)

    for scene in scenes:
        out_path = os.path.join(args.workdir, "{}.f32".format(scene))
        if os.path.exists(out_path):
            os.remove(out_path)
        code, log = reference.render(
            args.renderer, reference.scene_config(args.reference_dir, scene),
            out_path)
        if code == SKIP:
            # A skip is only honest if nothing has failed yet; every failure
            # above returns immediately, so no failure can be masked here.
            print("SKIPPED: no GL 4.3 compute context ({})".format(scene))
            if log.strip():
                print(log.strip())
            return SKIP
        if code != 0:
            print("FAIL: {}: renderer exited {}".format(scene, code),
                  file=sys.stderr)
            print(log, file=sys.stderr)
            return 1
        if not os.path.exists(out_path):
            print("FAIL: {}: renderer wrote no output".format(scene),
                  file=sys.stderr)
            return 1

        defects, (width, height) = check_buffer(out_path)
        if defects:
            for line in defects:
                print("FAIL: {}: {}".format(scene, line), file=sys.stderr)
            return 1
        print("PASS {}: {}x{} finite, in [0, 1+{}]".format(
            scene, width, height, CEILING_SLACK))

    print("PASS: {} scene(s) numerically sane".format(len(scenes)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
