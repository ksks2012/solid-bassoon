#ifndef HEADLESS_H
#define HEADLESS_H

#include <string>

// ── Headless automation entry point ───────────────────────────────────────────
// Renders a single frame with a hidden window and writes the post-tone-map
// RGBA32F output to disk. Used by the golden-regression harness (ticket 0011).

namespace headless {
constexpr int kOk   = 0;  // frame rendered and written
constexpr int kFail = 1;  // config/shader/IO error — a real failure
constexpr int kSkip = 2;  // no GL 4.3 + compute context — reported, not passed
}  // namespace headless

// Returns one of headless::{kOk,kFail,kSkip}. The output is the raw
// little-endian float32 RGBA buffer (width*height*4 floats, bottom-to-top),
// with a `<out>.meta` sidecar recording its shape.
int runHeadless(const std::string& configPath, const std::string& outPath);

// ── Emission-kernel probe (ticket 0012, spec/0002 seam S2) ────────────────────
// Dispatches shaders/probe.comp over the shared probe table
// (include/core/probe_cases.h) with a hidden window — no frame is rendered —
// and writes the kernel outputs. Returns headless::{kOk,kFail,kSkip}. The
// output is numCases·4 little-endian float32 records with a `<out>.meta`
// sidecar, structurally identical to the CPU reference's output so
// tests/gpu/probe_check.py can compare the two directly.
int runProbe(const std::string& outPath);

// ── Benchmark entry point (ticket 0013, spec/0002 performance protocol) ───────
// Times `frames` compute dispatches of `configPath` in a hidden window after
// `warmup` discarded ones, using a GL_TIME_ELAPSED query — GPU time for
// tracing the frame, with no present pass. A further pass with the shader's
// per-step counters switched on supplies the derived diagnostic (average RK4
// steps per ray) and the cost of the counter itself; that cost is reported as
// stats_overhead_pct and is never part of the gated measurement.
// Prints the CSV header and one row to stdout — the header line is the marker
// the performance gate looks for among the log output — and returns
// headless::{kOk,kFail,kSkip}.
int runBench(const std::string& configPath, int warmup, int frames);

#endif // HEADLESS_H
