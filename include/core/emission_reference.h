#ifndef BLACKHOLE_CORE_EMISSION_REFERENCE_H
#define BLACKHOLE_CORE_EMISSION_REFERENCE_H

// ═══════════════════════════════════════════════════════════════════════════════
// CPU reference for the GPU-only emission kernels (ticket 0012, seam S1)
//
// The jet density, jet beaming, disk emission and tone-map kernels lived only
// inside `shaders/raytrace.comp` (CONTEXT §5.8), so nothing could verify them
// without a live GL context.  This is an independent C++ reimplementation of
// those four kernels: it shares no code with the shader, is double precision
// (so it does not merely mirror the shader's float rounding), and is pure — no
// GL, no globals, no uniform state (spec/0002 §"Seams").
//
// `blackhole_core` links this (ADR-0005), so it is unit-testable on any machine.
// The GPU side is checked against it by `tests/gpu/probe_check.py` (seam S3).
//
// Formula provenance — each kernel is written from its documented definition,
// not transliterated from the shader:
//   diskEmission    CONTEXT §4.5 (opaque disk), §4.6 (redshift/Doppler), §5.8
//   jetDensity      ADR-0003 (emission-only), CONTEXT §2 "Relativistic jet"
//   jetBeamFactor   ADR-0004, CONTEXT §4.8 (δ^(3+α), emitter→observer = −n̂)
//   toneMap         ADR-0001 (identity below knee 0.8, width 0.2 → ceiling 1.0)
// ═══════════════════════════════════════════════════════════════════════════════

namespace emission {

// Linear HDR / display colour triple.
struct Rgb {
    double r, g, b;
};

// Transverse + axial shape of the relativistic jet (ADR-0003).
struct JetShape {
    double base_radius;       // cone foot radius at z = 0
    double opening_angle_deg; // half-opening angle (degrees)
    double profile_exponent;  // 1 = cone, 0.5 = parabola
    double max_height;        // axial extent; emission fades to 0 across it
    double radial_sigma;      // transverse Gaussian width
    double z_falloff;         // axial falloff length scale
    double axial_falloff;     // axial power-law exponent
};

// Constant bulk velocity producing Doppler beaming (ADR-0004).
struct JetBeam {
    double bulk_velocity;   // β
    double spectral_index;  // α in δ^(3+α)
};

// Display transform parameters (ADR-0001).
struct ToneMap {
    double exposure;  // linear exposure multiplier
    double gamma;     // display gamma (1.0 = identity)
};

// Transverse Gaussian × bounded axial power law × soft top, for a sample at
// cylindrical (r, θ) with the enclosing-sphere early-out bound `rBound`.
// Returns exactly 0 outside the solid of revolution, and never negative.
double jetDensity(double r, double theta, double rBound, const JetShape& shape);

// δ^(3+α) for a ray carrying (pos, mom) and metric factor `f`.
// `pos` = (t, r, θ, φ), `mom` = (p_t, p_r, p_θ, p_φ) with p_t normalised to -1.
// Returns exactly 1 when β = 0 (unbeamed) and never negative.
double jetBeamFactor(const double pos[4], const double mom[4], double f,
                     const JetBeam& beam);

// Disk radiance at (r, φ) for an observer at `inclination_deg`, including the
// Novikov-Thorne-ish temperature profile, gravitational redshift √f and the
// Keplerian Doppler³ boost. Never negative, never NaN/Inf.
Rgb diskEmission(double r, double phi, double rs, double rIn, double rOut,
                 double tempScale, double inclinationDeg);

// Linear HDR → display LDR: exposure, soft-knee roll-off (identity below the
// knee, asymptotic ceiling at knee + width), then gamma. Never above the
// ceiling and never negative.
Rgb toneMap(const Rgb& linear, const ToneMap& params);

} // namespace emission

#endif // BLACKHOLE_CORE_EMISSION_REFERENCE_H
