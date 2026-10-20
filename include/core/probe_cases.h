#ifndef BLACKHOLE_CORE_PROBE_CASES_H
#define BLACKHOLE_CORE_PROBE_CASES_H

// ═══════════════════════════════════════════════════════════════════════════════
// Shared probe input table (ticket 0012, spec/0002 seams S2 + S3)
//
// One table of fixed probe inputs, consumed by *both* sides of the
// GPU ↔ CPU cross-validation:
//
//   shaders/probe.comp           (via `BlackholeRenderer --probe <out>`)
//   tests/probe_reference        (core-only CPU reference, `--out <out>`)
//
// `tests/gpu/probe_check.py` runs both, compares the two raw float32 outputs
// and fails on any disagreement beyond the cross-precision tolerance.
//
// The table deliberately covers the properties named in ticket 0012:
//   · diskEmission   r^-3/4 radial shape (far-radius pair) and the Doppler
//                    asymmetry between the approaching and receding sides
//   · jetDensity     axial falloff literals, all three bounding cuts, the
//                    transverse Gaussian, the non-default parabolic profile,
//                    and the no-central-blow-up point
//   · jetBeamFactor  β = 0 unbeamed, the head-on/receding worked example,
//                    and a |β| sweep (monotonicity), plus the sign trap
//   · toneMap        identity below the knee, the knee itself, the ceiling
//                    above it, and exposure/gamma variants
//
// Inputs are stored as float32 so both implementations see *bit-identical*
// inputs; the CPU reference then evaluates them in double.
//
// Kernel ids and record strides must match `shaders/probe.comp`.  A mismatch
// cannot pass silently: the two sides would compute different kernels and
// disagree by orders of magnitude.
// ═══════════════════════════════════════════════════════════════════════════════

#include "core/emission_reference.h"

#include <array>
#include <cstddef>
#include <fstream>
#include <string>
#include <vector>

namespace probe {

// ── Kernel ids (shared with shaders/probe.comp) ───────────────────────────────
inline constexpr int kDiskEmission  = 0;
inline constexpr int kJetDensity    = 1;
inline constexpr int kJetBeamFactor = 2;
inline constexpr int kToneMap       = 3;

// ── Record layout (float32) ───────────────────────────────────────────────────
//   input   kInputStride  floats: [0] = kernel id, [1..3] = pad,
//                         [4..7] = a, [8..11] = b, [12..15] = c
//   output  kOutputStride floats: [0..2] = result xyz, [3] = 0
inline constexpr int kInputStride  = 16;
inline constexpr int kOutputStride = 4;

struct Case {
    int                  kernel;
    std::array<float, 12> params;  // a ‖ b ‖ c
};

// ── Packagers ─────────────────────────────────────────────────────────────────
// Each helper documents which logical argument lands in which slot; the GPU
// side reconstructs the same argument order (see shaders/probe.comp).

constexpr Case diskEmission(float r, float phi, float rs, float rIn, float rOut,
                            float tempScale, float inclinationDeg) {
    // a = (r, φ, rs, r_in)   b = (r_out, temp_scale, inclination, —)
    return {kDiskEmission,
            {r, phi, rs, rIn, rOut, tempScale, inclinationDeg, 0.0f, 0, 0, 0, 0}};
}

constexpr Case jetDensity(float r, float theta, float rBound,
                          float baseRadius, float openingAngleDeg,
                          float profileExponent, float maxHeight,
                          float radialSigma, float zFalloff, float axialFalloff) {
    // a = (r, θ, rBound, base_radius)
    // b = (opening_angle_deg, profile_exponent, max_height, radial_sigma)
    // c = (z_falloff, axial_falloff, —, —)
    return {kJetDensity,
            {r, theta, rBound, baseRadius, openingAngleDeg, profileExponent,
             maxHeight, radialSigma, zFalloff, axialFalloff, 0.0f, 0.0f}};
}

constexpr Case jetBeamFactor(std::array<float, 4> pos, std::array<float, 4> mom,
                             float f, float bulkVelocity, float spectralIndex) {
    // a = (t, r, θ, φ)   b = (p_t, p_r, p_θ, p_φ)   c = (f, β, α, —)
    return {kJetBeamFactor,
            {pos[0], pos[1], pos[2], pos[3], mom[0], mom[1], mom[2], mom[3],
             f, bulkVelocity, spectralIndex, 0.0f}};
}

constexpr Case toneMap(float r, float g, float b, float exposure, float gamma) {
    // a = (r, g, b, —)   b = (exposure, gamma, —, —)
    return {kToneMap, {r, g, b, 0.0f, exposure, gamma, 0.0f, 0.0f, 0, 0, 0, 0}};
}

// ── The table ─────────────────────────────────────────────────────────────────
inline constexpr float kPi = 3.14159265358979323846f;

// Shipping jet profile (RayTracerParams::jet) used for most jet cases; a
// deliberately different parabolic profile covers the `profile_exponent != 1`
// branch that the defaults never exercise.
inline constexpr float kBaseRadius      = 3.2f;
inline constexpr float kOpeningAngleDeg = 7.0f;
inline constexpr float kProfileExponent = 1.0f;
inline constexpr float kMaxHeight       = 40.0f;
inline constexpr float kRadialSigma     = 0.4f;
inline constexpr float kZFalloff        = 10.0f;
inline constexpr float kAxialFalloff    = 1.5f;

inline constexpr float kAltExponent   = 0.5f;
inline constexpr float kAltMaxHeight  = 25.0f;
inline constexpr float kAltOpening    = 12.0f;
inline constexpr float kAltSigma      = 0.6f;
inline constexpr float kAltZFalloff   = 6.0f;
inline constexpr float kAltAxialFall  = 2.0f;

constexpr Case defaultJet(float r, float theta, float rBound) {
    return jetDensity(r, theta, rBound, kBaseRadius, kOpeningAngleDeg,
                      kProfileExponent, kMaxHeight, kRadialSigma,
                      kZFalloff, kAxialFalloff);
}

constexpr Case altJet(float r, float theta, float rBound) {
    return jetDensity(r, theta, rBound, kBaseRadius, kAltOpening,
                      kAltExponent, kAltMaxHeight, kAltSigma,
                      kAltZFalloff, kAltAxialFall);
}

constexpr float kJetRBound = 100.0f;  // generous enclosing-sphere bound for probes

inline constexpr std::array<Case, 48> kCases = {{
    // ── diskEmission (12) ─────────────────────────────────────────────────────
    // Inner edge, face-on: temperature × gravitational redshift only.
    diskEmission(6.0f, 0.0f,  2.0f, 6.0f, 12.0f, 1.0f, 0.0f),
    // Same radius, other azimuths — face-on must be azimuth-independent.
    diskEmission(6.0f, 1.0f,  2.0f, 6.0f, 12.0f, 1.0f, 0.0f),
    diskEmission(6.0f, 2.0f,  2.0f, 6.0f, 12.0f, 1.0f, 0.0f),
    // Far-radius pair a factor 2 apart: their ratio isolates the r^-3/4 shape.
    diskEmission(6000.0f,  0.0f, 2.0f, 6.0f, 12.0f, 1.0f, 0.0f),
    diskEmission(12000.0f, 0.0f, 2.0f, 6.0f, 12.0f, 1.0f, 0.0f),
    // Edge-on pair: approaching vs receding Doppler boost.
    diskEmission(12.0f, kPi * 0.5f,       2.0f, 6.0f, 12.0f, 1.0f, 90.0f),
    diskEmission(12.0f, kPi * 1.5f,       2.0f, 6.0f, 12.0f, 1.0f, 90.0f),
    // Mixed mid-disk samples.
    diskEmission(9.0f,  0.7f, 2.0f, 6.0f, 12.0f, 1.0f,  60.0f),
    diskEmission(11.0f, 2.3f, 2.0f, 6.0f, 12.0f, 1.0f,  45.0f),
    diskEmission(12.0f, 1.1f, 2.0f, 6.0f, 12.0f, 1.0f,  30.0f),
    // Temperature scale variants (stays below the intensity clamp of 5.0).
    diskEmission(6.0f,  0.0f, 2.0f, 6.0f, 12.0f, 3.0f,  0.0f),
    diskEmission(7.5f, 4.0f, 2.0f, 6.0f, 12.0f, 1.5f, 75.0f),

    // ── jetDensity (16) ───────────────────────────────────────────────────────
    // On-axis axial falloff at three heights (the hand-derived literals).
    defaultJet(10.0f, 0.0f, kJetRBound),
    defaultJet(20.0f, 0.0f, kJetRBound),
    defaultJet(30.0f, 0.0f, kJetRBound),
    // All three bounding cuts: axial, radial, transverse.
    defaultJet(40.0f, 0.0f, kJetRBound),   // az >= max_height
    defaultJet(45.0f, 0.0f, kJetRBound),   // beyond the soft top
    defaultJet(kJetRBound, 0.0f, kJetRBound),  // r >= rBound
    defaultJet(10.0f, kPi * 0.5f, kJetRBound), // transverse cut at the base
    // Transverse Gaussian across the cone, including just inside the cut.
    defaultJet(1.0f,  kPi * 0.5f, kJetRBound),
    defaultJet(3.0f,  kPi * 0.5f, kJetRBound),
    defaultJet(3.7f,  kPi * 0.5f, kJetRBound),
    // No central blow-up (finite, near 1).
    defaultJet(0.001f, kPi * 0.5f, kJetRBound),
    // Oblique samples inside the cone.
    defaultJet(15.0f, 1.0f,  kJetRBound),
    defaultJet(8.0f,  0.4f,  kJetRBound),
    defaultJet(25.0f, 2.5f,  kJetRBound),
    // Non-default parabolic profile (profile_exponent != 1 → pow branch).
    altJet(5.0f,  0.9f, kJetRBound),
    altJet(18.0f, 0.25f, kJetRBound),

    // ── jetBeamFactor (10) ────────────────────────────────────────────────────
    // Head-on worked example (θ = 0, f = 0.5, n̂_x = f·p_r = -1).
    jetBeamFactor({0.0f, 10.0f, 0.0f, 0.0f}, {-1.0f, -2.0f, 0.0f, 0.0f},
                  0.5f, 0.6f, 0.7f),
    // The same geometry with the march reversed — the emitter→observer sign.
    jetBeamFactor({0.0f, 10.0f, 0.0f, 0.0f}, {-1.0f, 2.0f, 0.0f, 0.0f},
                  0.5f, 0.6f, 0.7f),
    // β = 0 must be exactly unbeamed.
    jetBeamFactor({0.0f, 10.0f, 0.0f, 0.0f}, {-1.0f, -2.0f, 0.0f, 0.0f},
                  0.5f, 0.0f, 0.7f),
    // |β| sweep — gain must rise monotonically toward the observer and fall
    // away from it.
    jetBeamFactor({0.0f, 10.0f, 0.0f, 0.0f}, {-1.0f, -2.0f, 0.0f, 0.0f},
                  0.5f, 0.1f, 0.7f),
    jetBeamFactor({0.0f, 10.0f, 0.0f, 0.0f}, {-1.0f, -2.0f, 0.0f, 0.0f},
                  0.5f, 0.5f, 0.7f),
    jetBeamFactor({0.0f, 10.0f, 0.0f, 0.0f}, {-1.0f, -2.0f, 0.0f, 0.0f},
                  0.5f, 0.9f, 0.7f),
    // General geometry with all momentum components live.
    jetBeamFactor({0.0f, 12.0f, 0.9f, 1.0f}, {-1.0f, -1.5f, 0.3f, 0.7f},
                  0.6f, 0.95f, 0.7f),
    jetBeamFactor({0.0f, 12.0f, 0.9f, 1.0f}, {-1.0f, -1.5f, 0.3f, 0.7f},
                  0.6f, -0.95f, 0.7f),
    // Clamped edge: |β| at the 0.999 limit.
    jetBeamFactor({0.0f, 12.0f, 0.9f, 1.0f}, {-1.0f, -1.5f, 0.3f, 0.7f},
                  0.6f, 0.999f, 0.7f),
    // Equatorial sample with large p_φ (the r·sinθ denominator).
    jetBeamFactor({0.0f, 20.0f, kPi * 0.5f, 0.0f}, {-1.0f, -1.0f, 0.0f, 5.0f},
                  0.9f, 0.8f, 0.7f),

    // ── toneMap (10) ──────────────────────────────────────────────────────────
    // Identity below the knee.
    toneMap(0.5f, 0.0f, 0.7f, 1.0f, 1.0f),
    // Exactly at the knee.
    toneMap(0.8f, 0.8f, 0.8f, 1.0f, 1.0f),
    // Exposure worked example: 0.5 × 2 → 0.9.
    toneMap(0.5f, 0.5f, 0.5f, 2.0f, 1.0f),
    // Above the knee — must stay ≤ 1.0.
    toneMap(1.0f, 1.0f, 1.0f, 1.0f, 1.0f),
    toneMap(1000.0f, 1.0f, 0.0f, 1.0f, 1.0f),
    // Zero and dark inputs (clamp path).
    toneMap(0.0f, 0.0f, 0.0f, 1.0f, 1.0f),
    toneMap(0.25f, 0.25f, 0.25f, 1.0f, 1.0f),
    // Gamma variants.
    toneMap(0.25f, 0.25f, 0.25f, 1.0f, 2.0f),
    toneMap(0.5f, 0.5f, 0.5f, 1.0f, 0.45f),
    // Exposure below unity.
    toneMap(3.0f, 3.0f, 3.0f, 0.5f, 1.0f),
}};

// ── Flattened SSBO image ──────────────────────────────────────────────────────
inline std::array<float, kInputStride> packInput(const Case& c) {
    std::array<float, kInputStride> rec{};
    rec[0] = static_cast<float>(c.kernel);
    for (int i = 0; i < 12; ++i) rec[i + 4] = c.params[static_cast<std::size_t>(i)];
    return rec;
}

inline std::vector<float> packInputs() {
    std::vector<float> out;
    out.reserve(kCases.size() * kInputStride);
    for (const Case& c : kCases) {
        const std::array<float, kInputStride> rec = packInput(c);
        out.insert(out.end(), rec.begin(), rec.end());
    }
    return out;
}

// ── CPU evaluation of one case (emission_reference.h) ────────────────────────
inline emission::Rgb evaluate(const Case& c) {
    const std::array<float, 12>& p = c.params;
    const double a[4] = {p[0], p[1], p[2], p[3]};
    const double b[4] = {p[4], p[5], p[6], p[7]};
    const double g[4] = {p[8], p[9], p[10], p[11]};

    switch (c.kernel) {
        case kDiskEmission:
            return emission::diskEmission(a[0], a[1], a[2], a[3],
                                          b[0], b[1], b[2]);
        case kJetDensity: {
            const emission::JetShape s{a[3], b[0], b[1], b[2], b[3], g[0], g[1]};
            return {emission::jetDensity(a[0], a[1], a[2], s), 0.0, 0.0};
        }
        case kJetBeamFactor:
            return {emission::jetBeamFactor(a, b, g[0], {g[1], g[2]}), 0.0, 0.0};
        case kToneMap:
            return emission::toneMap({a[0], a[1], a[2]}, {b[0], b[1]});
        default:
            return {0.0, 0.0, 0.0};
    }
}

// ── Output format ─────────────────────────────────────────────────────────────
// The single definition of a probe result file: `numCases · kOutputStride`
// little-endian float32 plus a `<path>.meta` sidecar.  Both writers — this
// CPU reference and `BlackholeRenderer --probe` — go through here, so the two
// files are structurally identical by construction and `tests/gpu/compare.py`
// can validate them with its usual `width × height × channels` shape check
// (the probe records the case index as `width` and one result row as `height`).
inline bool writeProbeFile(const std::string& path, const std::vector<float>& results,
                           int numCases, const std::string& source) {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) return false;
    if (!results.empty()) {
        out.write(reinterpret_cast<const char*>(results.data()),
                  static_cast<std::streamsize>(results.size() * sizeof(float)));
    }
    if (!out.good()) return false;

    std::ofstream meta(path + ".meta");
    if (!meta.is_open()) return false;
    meta << "width=" << numCases << "\n"
         << "height=1\n"
         << "channels=" << kOutputStride << "\n"
         << "case_count=" << numCases << "\n"
         << "result_stride=" << kOutputStride << "\n"
         << "dtype=float32\n"
         << "layout=probe-vec4\n"
         << "byte_order=little\n"
         << "source=" << source << "\n";
    return meta.good();
}

} // namespace probe

#endif // BLACKHOLE_CORE_PROBE_CASES_H
