#!/usr/bin/env python3
"""Headless render smoke + determinism check (ticket 0010).

Runs the renderer twice with identical inputs and asserts the raw float32
output buffers are byte-identical. If the renderer reports that no GL 4.3
compute context is available (exit code 2) this exits 2 as well, so CTest
(configured with SKIP_RETURN_CODE 2) reports it as *skipped* — an unrun check
is never turned into a pass.

Usage:
    headless_check.py <renderer> <config.json> <out_a> <out_b>
"""
import os
import subprocess
import sys

SKIP = 2


def run_once(renderer, config, out_path):
    if os.path.exists(out_path):
        os.remove(out_path)
    proc = subprocess.run(
        [renderer, "--render", config, "--out", out_path],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    return proc


def read_meta(path):
    meta = {}
    with open(path) as handle:
        for line in handle:
            line = line.strip()
            if "=" in line:
                key, value = line.split("=", 1)
                meta[key.strip()] = value.strip()
    return meta


def main():
    if len(sys.argv) != 5:
        print("usage: headless_check.py <renderer> <config> <out_a> <out_b>",
              file=sys.stderr)
        return 1

    renderer, config, out_a, out_b = sys.argv[1:5]

    for out_path in (out_a, out_b):
        proc = run_once(renderer, config, out_path)
        if proc.returncode == SKIP:
            print("SKIPPED: renderer reports no GL 4.3 compute context")
            if proc.stderr.strip():
                print(proc.stderr.strip())
            return SKIP
        if proc.returncode != 0:
            print("FAIL: renderer exited {}".format(proc.returncode), file=sys.stderr)
            print(proc.stdout, file=sys.stderr)
            print(proc.stderr, file=sys.stderr)
            return 1
        if not os.path.exists(out_path):
            print("FAIL: no output written to {}".format(out_path), file=sys.stderr)
            return 1

    with open(out_a, "rb") as fa, open(out_b, "rb") as fb:
        buf_a, buf_b = fa.read(), fb.read()

    # Validate the buffer against its sidecar so a wrong-sized output fails
    # loudly instead of being silently compared (standard §"Reference format").
    try:
        meta_a = read_meta(out_a + ".meta")
        meta_b = read_meta(out_b + ".meta")
    except OSError as exc:
        print("FAIL: cannot read sidecar: {}".format(exc), file=sys.stderr)
        return 1

    try:
        width = int(meta_a["width"])
        height = int(meta_a["height"])
        channels = int(meta_a["channels"])
    except (KeyError, ValueError) as exc:
        print("FAIL: sidecar missing/invalid shape key: {}".format(exc), file=sys.stderr)
        return 1

    expected_bytes = width * height * channels * 4  # float32 = 4 bytes
    if len(buf_a) != expected_bytes:
        print("FAIL: buffer is {} bytes, sidecar declares {}x{}x{} ({} bytes)".format(
            len(buf_a), width, height, channels, expected_bytes), file=sys.stderr)
        return 1
    if meta_a["width"] != meta_b["width"] or meta_a["height"] != meta_b["height"]:
        print("FAIL: the two runs disagree on shape", file=sys.stderr)
        return 1

    if len(buf_a) == 0:
        print("FAIL: empty output buffer", file=sys.stderr)
        return 1
    if not any(buf_a):
        print("FAIL: output buffer is all zeros (no frame rendered)", file=sys.stderr)
        return 1
    if buf_a != buf_b:
        for i, (x, y) in enumerate(zip(buf_a, buf_b)):
            if x != y:
                print("FAIL: buffers differ at byte {} ({} vs {})".format(i, x, y),
                      file=sys.stderr)
                break
        return 1

    # Byte-identical is the correct (not merely tolerated) assertion here: this
    # is the *deterministic-path* case of the standard's "Determinism and
    # tolerance" rule — same binary, same GPU, same inputs, no accumulation
    # across frames. Any divergence is nondeterminism, which is a defect.
    print("PASS: headless render deterministic across two runs ({} bytes)".format(len(buf_a)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
