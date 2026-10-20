#!/usr/bin/env python3
"""Shared helpers for the golden-regression tooling (ticket 0011).

Keeps scene discovery, renderer invocation, PPM export, and pixel-space diff
statistics in one place so the generator and the regression runner agree.
Stdlib only.
"""
import math
import os
import subprocess
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import compare  # noqa: E402

REPO_ROOT = os.path.abspath(os.path.join(_HERE, os.pardir, os.pardir))
DEFAULT_REFERENCE_DIR = os.path.join(REPO_ROOT, "tests", "reference")
DEFAULT_RENDERER = os.path.join(REPO_ROOT, "bin", "BlackholeRenderer")

# Comparison tolerance for a raw float32 golden.
#
# Justification (standards/testing-and-performance.md §3 "Justify every
# tolerance"): 1e-3 is ~1/4 of one 8-bit code value (1/255 = 3.9e-3), so a
# passing comparison is already invisible once the buffer is quantised. In
# practice identical builds on the reference machine agree exactly
# (max_abs = 0), so this is headroom for float32 re-evaluation of the
# transcendentals across runs, never headroom for a visual change.
# The jet-off scene uses 0.0: byte identity is the contract, not a tolerance.
DEFAULT_TOLERANCE = 1e-3

# Pixel threshold for "does this pixel differ" when computing the disk delta
# bounding box — same 1/4-code-value argument as above.
DEFAULT_THRESHOLD = 1e-3


def render(renderer, config, out_path, extra_env=None):
    """Run `renderer --render <config> --out <out_path>`; return (code, log).

    Exit code 2 is the renderer's honest "no usable GPU" skip (ticket 0010).
    """
    env = dict(os.environ)
    if extra_env:
        env.update(extra_env)
    proc = subprocess.run([renderer, "--render", config, "--out", out_path],
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          env=env)
    return proc.returncode, proc.stdout.decode("utf-8", "replace")


def scenes_dir(reference_dir):
    return os.path.join(reference_dir, "scenes")


def scene_names(reference_dir):
    """Sorted scene names (without .json) found in <reference_dir>/scenes."""
    directory = scenes_dir(reference_dir)
    return sorted(name[:-5] for name in os.listdir(directory) if name.endswith(".json"))


def scene_config(reference_dir, scene):
    return os.path.join(scenes_dir(reference_dir), scene + ".json")


def scene_path(reference_dir, scene, suffix):
    return os.path.join(reference_dir, scene + suffix)


def write_ppm(f32_path, ppm_path, gamma=2.2):
    """Write a viewable P6 PPM from a raw float32 buffer (origin flipped)."""
    data = compare.load_f32(f32_path)
    meta = compare.read_meta(f32_path + ".meta")
    width, height, channels = compare.shape_from_meta(meta, len(data), f32_path)
    if channels != 4:
        raise ValueError("{}: expected 4 channels, got {}".format(f32_path, channels))
    inverse = 1.0 / gamma
    rows = []
    for y in range(height - 1, -1, -1):  # origin=bottom-left -> top-left PPM
        base = y * width * 4
        row = bytearray()
        for x in range(width):
            offset = base + x * 4
            for k in range(3):
                v = data[offset + k]
                if not math.isfinite(v) or v <= 0.0:
                    q = 0
                elif v >= 1.0:
                    q = 255
                else:
                    q = int(round((v ** inverse) * 255.0))
                row.append(q)
        rows.append(bytes(row))
    with open(ppm_path, "wb") as handle:
        handle.write("P6\n{} {}\n255\n".format(width, height).encode("ascii"))
        handle.write(b"".join(rows))


def pixel_diff(a, b, width, height, threshold):
    """Pixel-space stats: count of differing pixels and their bounding box."""
    count = 0
    min_x = min_y = 1 << 30
    max_x = max_y = -1
    for p in range(width * height):
        offset = p * 4
        worst = 0.0
        for k in range(4):
            x = a[offset + k]
            y = b[offset + k]
            if math.isfinite(x) and math.isfinite(y):
                d = abs(x - y)
            else:
                d = math.inf
            if d > worst:
                worst = d
        if worst > threshold:
            count += 1
            px = p % width
            py = p // width
            min_x = min(min_x, px)
            max_x = max(max_x, px)
            min_y = min(min_y, py)
            max_y = max(max_y, py)
    bbox = None if count == 0 else (min_x, min_y, max_x, max_y)
    return {"count": count, "bbox": bbox}


def bbox_within(inner, outer, margin=0):
    """True when inner bbox lies inside outer bbox expanded by `margin` pixels."""
    if inner is None:
        return True
    ix0, iy0, ix1, iy1 = inner
    ox0, oy0, ox1, oy1 = outer
    return (ix0 >= ox0 - margin and iy0 >= oy0 - margin
            and ix1 <= ox1 + margin and iy1 <= oy1 + margin)


def code_hash(reference_dir, scene):
    """SHA-256 over everything that can change this scene's output.

    Used to enforce the golden-update policy (standards §3): a reference that
    changes while this hash does not is a defect — nondeterminism or a bug —
    not a legitimate refresh.
    """
    import hashlib

    paths = []
    shaders = os.path.join(REPO_ROOT, "shaders")
    paths += [os.path.join(shaders, n) for n in sorted(os.listdir(shaders))
              if n.endswith((".comp", ".vert", ".frag"))]
    for rel in ("src/rendering/ray_tracer.cpp", "include/rendering/ray_tracer.h",
                "include/rendering/accretion_disk.h",
                "src/app/render_params.cpp", "include/app/render_params.h",
                "src/app/config.cpp", "include/app/config.h"):
        paths.append(os.path.join(REPO_ROOT, rel))
    paths.append(scene_config(reference_dir, scene))

    digest = hashlib.sha256()
    for path in paths:
        digest.update(os.path.basename(path).encode("utf-8"))
        try:
            with open(path, "rb") as handle:
                digest.update(handle.read())
        except OSError:
            digest.update(b"<missing>")
    return digest.hexdigest()


_GL_RENDERER_RE = None


def gl_renderer_from_log(log):
    """Extract the GL_RENDERER string from a renderer log ("Renderer: ...").

    Returns None when the log carries no such line, so callers can fall back to
    an explicit SKIP rather than a confusing numeric failure.
    """
    global _GL_RENDERER_RE
    if _GL_RENDERER_RE is None:
        import re
        _GL_RENDERER_RE = re.compile(r"Renderer:\s*(.+)")
    for line in log.splitlines():
        match = _GL_RENDERER_RE.search(line)
        if match:
            return match.group(1).strip()
    return None
