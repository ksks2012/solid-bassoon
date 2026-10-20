#include "check.h"

#include "core/probe_cases.h"

#include <array>
#include <cstdio>
#include <string>
#include <vector>

// ── CPU side of the GPU ↔ CPU cross-validation (ticket 0012, seam S3) ─────────
//
// `probe_reference` links `blackhole_core` only — no GL, no window (ADR-0005).
// It is the reference half of the probe pair:
//
//   bin/probe_reference --out <file>   CPU results (this executable)
//   BlackholeRenderer --probe <file>   GPU results (shaders/probe.comp)
//   tests/gpu/probe_check.py           runs both and compares them
//
// Run with no arguments it instead checks the *universal* invariants of the
// shared table — finite, non-negative, the tone-map ceiling, and the
// identity-below-the-knee contract — over exactly the inputs the GPU is held
// to.  The discriminating literal assertions (falloff values, δ^(3+α) worked
// examples, r^-3/4 ratios) live in `tests/unit/emission_reference_test.cpp`
// against hand-derived numbers; this executable deliberately does not repeat
// them, so a CPU/GPU agreement can never be mistaken for independent evidence.
//
// Exit code 0 = pass, 1 = fail.  No skip state: this test needs no GL.

namespace {

// Encode one CPU result as the fixed four-float record the GPU also writes.
std::vector<float> evaluateAll() {
    std::vector<float> out;
    out.reserve(probe::kCases.size() * probe::kOutputStride);
    for (const probe::Case& c : probe::kCases) {
        const emission::Rgb rgb = probe::evaluate(c);
        out.push_back(static_cast<float>(rgb.r));
        out.push_back(static_cast<float>(rgb.g));
        out.push_back(static_cast<float>(rgb.b));
        out.push_back(0.0f);
    }
    return out;
}

void checkTableInvariants() {
    const std::vector<float> inputs = probe::packInputs();
    CHECK(!probe::kCases.empty());
    CHECK(inputs.size() == probe::kCases.size() * probe::kInputStride);

    bool allFinite = true;
    bool allNonNegative = true;
    bool toneCeilingHolds = true;
    bool toneIdentityHolds = true;

    for (const probe::Case& c : probe::kCases) {
        CHECK(c.kernel >= probe::kDiskEmission && c.kernel <= probe::kToneMap);

        const emission::Rgb rgb = probe::evaluate(c);
        for (double v : {rgb.r, rgb.g, rgb.b}) {
            if (!std::isfinite(v)) allFinite = false;
            if (v < 0.0) allNonNegative = false;
        }

        if (c.kernel != probe::kToneMap) continue;

        const double exposure = c.params[4];
        const double gamma    = c.params[5];
        for (double v : {rgb.r, rgb.g, rgb.b}) {
            if (v > 1.0) toneCeilingHolds = false;
        }
        // Identity below the knee, for the plain (exposure 1, gamma 1) rows:
        // the contract asserted by ADR-0001 and ticket 0012 item 4, checked
        // here over the shared probe inputs rather than hand-picked ones.
        const bool plain = exposure == 1.0 && gamma == 1.0;
        const bool belowKnee = c.params[0] < 0.8 && c.params[1] < 0.8 &&
                               c.params[2] < 0.8;
        if (plain && belowKnee) {
            if (rgb.r != c.params[0] || rgb.g != c.params[1] || rgb.b != c.params[2]) {
                toneIdentityHolds = false;
            }
        }
    }

    CHECK(allFinite);
    CHECK(allNonNegative);
    CHECK(toneCeilingHolds);
    CHECK(toneIdentityHolds);
}

} // namespace

int main(int argc, char* argv[]) {
    std::string outPath;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--out" && i + 1 < argc) {
            outPath = argv[++i];
        } else {
            std::fprintf(stderr, "usage: probe_reference [--out <file>]\n");
            return 1;
        }
    }

    checkTableInvariants();

    if (!outPath.empty()) {
        const std::vector<float> results = evaluateAll();
        if (!probe::writeProbeFile(outPath, results,
                                   static_cast<int>(probe::kCases.size()), "cpu")) {
            std::fprintf(stderr, "error: cannot write %s\n", outPath.c_str());
            return 1;
        }
        std::printf("PROBED %zu cases -> %s\n", probe::kCases.size(), outPath.c_str());
    }

    return check::report("test_probe_reference");
}
