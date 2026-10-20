#!/usr/bin/env python3
"""Generate/refresh the golden reference buffers (ticket 0011).

Rendering a golden is a deliberate act: this tool refuses to run without an
explicit `--reason` and records that reason, the source config, and the
renderer invocation in each sidecar. See tests/reference/README.md.

Usage:
    update_reference.py --reason "why" [scene ...] [--renderer PATH]
    update_reference.py --reason "why" --all [--renderer PATH]

When a reference already exists the previous-vs-new delta is printed so a
refresh is never silent.
"""
import argparse
import datetime
import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import compare  # noqa: E402
import reference  # noqa: E402


def write_sidecar(path, src_meta, config, reason, scene_hash, gl_renderer):
    keys = ["width", "height", "channels", "dtype", "layout", "byte_order", "origin"]
    lines = ["scene=" + os.path.splitext(os.path.basename(config))[0]]
    for key in keys:
        if key in src_meta:
            lines.append("{}={}".format(key, src_meta[key]))
    # Layer / seam / tolerance self-description (standards: every test artifact
    # states its layer, the seam it observes, and its tolerance).
    lines.append("layer=3-golden-regression")
    lines.append("seam=RayTracer::readback (glGetTexImage, RGBA32F)")
    lines.append("tolerance=" + repr(reference.DEFAULT_TOLERANCE))
    lines.append("view_gamma=2.2  # PPM only, never compared")
    lines.append("source=BlackholeRenderer --render {}".format(
        os.path.relpath(config, reference.REPO_ROOT)))
    lines.append("code_hash=" + scene_hash)
    lines.append("gl_renderer=" + (gl_renderer or "unknown"))
    lines.append("reason=" + reason)
    lines.append("updated=" + datetime.date.today().isoformat())
    with open(path, "w") as handle:
        handle.write("\n".join(lines) + "\n")


def update_scene(renderer, reference_dir, scene, reason, force=False):
    config = reference.scene_config(reference_dir, scene)
    out_f32 = reference.scene_path(reference_dir, scene, ".f32")
    out_ppm = reference.scene_path(reference_dir, scene, ".ppm")
    out_meta = out_f32 + ".meta"
    staging = out_f32 + ".new"

    code, log = reference.render(renderer, config, staging)
    if code == 2:
        print("SKIP {}: renderer has no usable GPU ({})".format(scene, log.strip()))
        return 2
    if code != 0:
        print("FAIL {}: renderer exited {}\n{}".format(scene, code, log.strip()),
              file=sys.stderr)
        return 1
    if not os.path.exists(staging + ".meta"):
        print("FAIL {}: renderer produced no sidecar".format(scene), file=sys.stderr)
        return 1

    new_hash = reference.code_hash(reference_dir, scene)
    gl_renderer = reference.gl_renderer_from_log(log)
    if os.path.exists(out_f32):
        try:
            report = compare.compare_files(out_f32, staging,
                                           threshold=reference.DEFAULT_THRESHOLD)
            print("  refresh {}: {}".format(scene, compare.format_report(report)))
            old_meta = compare.read_meta(out_f32 + ".meta")
            old_hash = old_meta.get("code_hash")
            old_gl = old_meta.get("gl_renderer")
            # README rule 1: moving goldens to another GPU/driver is a deliberate,
            # recorded upgrade, not a side effect. Refuse without --force and say
            # why, rather than mislabelling the mismatch as a code defect below.
            if (old_gl and gl_renderer and old_gl != "unknown"
                    and old_gl != gl_renderer and not force):
                os.remove(staging)
                if os.path.exists(staging + ".meta"):
                    os.remove(staging + ".meta")
                print(
                    "FAIL {}: GL driver mismatch — references were generated on "
                    "'{}', you are on '{}'. Regenerating here replaces every "
                    "golden with driver-specific data (README rule 1); rerun "
                    "with --force if that is the recorded upgrade you intend."
                    .format(scene, old_gl, gl_renderer), file=sys.stderr)
                return 1
            # standards §3: "A reference that changes without a code change is a
            # bug."  Same code hash + different bytes = nondeterminism or a bug,
            # so this is an error unless the caller explicitly owns the call.
            if old_hash == new_hash and report["max_abs"] != 0.0 and not force:
                os.remove(staging)
                if os.path.exists(staging + ".meta"):
                    os.remove(staging + ".meta")
                print(
                    "FAIL {}: reference changed while code_hash={} is unchanged "
                    "(max_abs={}). That is a defect, not a refresh — rerun with "
                    "--force if you have established this is a legitimate, "
                    "recorded change.".format(scene, new_hash[:12], report["max_abs"]),
                    file=sys.stderr)
                return 1
        except (OSError, ValueError) as exc:
            print("  refresh {}: cannot diff previous reference ({})".format(scene, exc))

    os.replace(staging, out_f32)
    os.replace(staging + ".meta", out_meta)
    write_sidecar(out_meta, compare.read_meta(out_meta), config, reason,
                  new_hash, gl_renderer)
    reference.write_ppm(out_f32, out_ppm)
    print("  wrote {} (.f32/.ppm/.meta)".format(scene))
    return 0


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--reason", required=True,
                        help="why the reference is being (re)generated (recorded in .meta)")
    parser.add_argument("--renderer", default=reference.DEFAULT_RENDERER)
    parser.add_argument("--reference-dir", default=reference.DEFAULT_REFERENCE_DIR)
    parser.add_argument("--all", action="store_true", help="regenerate every scene")
    parser.add_argument("--force", action="store_true",
                        help="allow a reference to change although code_hash is "
                             "unchanged (normally a defect, see README rule 8)")
    parser.add_argument("scenes", nargs="*")
    args = parser.parse_args(argv)

    if not os.path.exists(args.renderer):
        print("FAIL: renderer not found: {}".format(args.renderer), file=sys.stderr)
        return 1

    if args.all:
        scenes = reference.scene_names(args.reference_dir)
    elif args.scenes:
        scenes = args.scenes
    else:
        parser.error("name at least one scene or pass --all")

    failed = False
    skipped = False
    for scene in scenes:
        print("generating {}...".format(scene))
        code = update_scene(args.renderer, args.reference_dir, scene,
                            args.reason, force=args.force)
        if code == 2:
            skipped = True
        elif code != 0:
            failed = True
    # A failure must never be masked by a later skip.
    if failed:
        return 1
    return 2 if skipped else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
