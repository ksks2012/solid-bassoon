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

#endif // HEADLESS_H
