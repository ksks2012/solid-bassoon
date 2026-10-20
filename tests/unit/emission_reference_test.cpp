#include "check.h"

#include "core/emission_reference.h"

#include <cmath>

// ── Seam S1: CPU reference for the GPU-only emission kernels (ticket 0012) ────
//
// `emission_reference` is an independent C++ reimplementation of the four
// kernels that previously existed only inside `shaders/raytrace.comp`
// (CONTEXT §5.8): disk emission, jet density, jet beaming and the tone map.
//
// Every expected value below is a *worked example* or a *known-good literal*
// derived by hand from the documented formulas (CONTEXT §4/§6, ADR-0001,
// ADR-0003, ADR-0004) — never the code's own arithmetic echoed back.  That is
// what keeps these assertions able to disagree with the implementation.

namespace {

constexpr double kPi     = 3.14159265358979323846;
constexpr double kHalfPi = kPi * 0.5;

// Jet profile defaults — the shipping values from `RayTracerParams::jet`
// (include/rendering/ray_tracer.h), used as the fixed probe shape.
const emission::JetShape kJet{3.2, 7.0, 1.0, 40.0, 0.4, 10.0, 1.5};
constexpr double kBigBound = 100.0;

double jetDensity(double r, double theta) {
    return emission::jetDensity(r, theta, kBigBound, kJet);
}

} // namespace

int main() {
    // ════════════════════════════════════════════════════════════════════════
    // Tone map — ADR-0001: identity below `knee = 0.8`, compressing only
    // `[knee, knee + width]` with `width = 0.2`, so the ceiling is 1.0.
    // ════════════════════════════════════════════════════════════════════════
    {
        const emission::ToneMap flat{1.0, 1.0};  // exposure 1, gamma 1 = identity

        // Identity below the knee (gamma 1 is the documented default).
        const emission::Rgb below = emission::toneMap({0.5, 0.0, 0.7}, flat);
        CHECK_CLOSE(below.r, 0.5, 1e-12);
        CHECK_CLOSE(below.g, 0.0, 1e-12);
        CHECK_CLOSE(below.b, 0.7, 1e-12);

        // Exactly at the knee the operator is still the identity.
        const emission::Rgb at = emission::toneMap({0.8, 0.8, 0.8}, flat);
        CHECK_CLOSE(at.r, 0.8, 1e-12);
        CHECK_CLOSE(at.g, 0.8, 1e-12);
        CHECK_CLOSE(at.b, 0.8, 1e-12);

        // Worked example for `exposure`: c = 0.5 × 2 = 1.0, over = 0.2,
        //   mapped = 0.8 + 0.2 × 0.2 / (0.2 + 0.2) = 0.8 + 0.1 = 0.9
        const emission::Rgb scaled = emission::toneMap({0.5, 0.5, 0.5}, {2.0, 1.0});
        CHECK_CLOSE(scaled.r, 0.9, 1e-12);
        CHECK_CLOSE(scaled.g, 0.9, 1e-12);
        CHECK_CLOSE(scaled.b, 0.9, 1e-12);

        // Ceiling: above the knee the result stays ≤ knee + width = 1.0 and,
        // being an asymptote, never actually reaches it.
        const emission::Rgb hot = emission::toneMap({1000.0, 1.0, 0.0}, flat);
        CHECK(hot.r > 0.8);
        CHECK(hot.r <= 1.0);
        CHECK(hot.r < 1.0);
        CHECK_CLOSE(hot.g, 0.9, 1e-12);   // c = 1.0 → same worked example
        CHECK_CLOSE(hot.b, 0.0, 1e-12);   // c = 0.0 → below the knee

        // Sweep: never above the ceiling, never negative, monotone non-decreasing.
        double prev = -1.0;
        bool monotone = true;
        for (double c = 0.0; c <= 64.0; c += 0.25) {
            const emission::Rgb o = emission::toneMap({c, c, c}, flat);
            if (!(o.r <= 1.0)) monotone = false;
            if (!(o.r >= 0.0)) monotone = false;
            if (o.r < prev) monotone = false;
            prev = o.r;
        }
        CHECK(monotone);

        // Negative radiance is clamped before the knee (never propagates).
        const emission::Rgb neg = emission::toneMap({-3.0, -3.0, -3.0}, flat);
        CHECK_CLOSE(neg.r, 0.0, 0.0);

        // Gamma is a separate display encoding: pow(0.5, 1/2) = 0.7071067811865476.
        const emission::Rgb gam = emission::toneMap({0.5, 0.5, 0.5}, {1.0, 2.0});
        CHECK_CLOSE(gam.r, 0.7071067811865476, 1e-12);
    }

    // ════════════════════════════════════════════════════════════════════════
    // Jet beaming — δ^(3+α) from a constant bulk velocity (ADR-0004).
    //
    // Geometry used below: θ = 0, f = 0.5, so v̂ = +x̂ and n̂_x = f·p_r.
    // Choosing p_r = -2 gives n̂_x = -1, i.e. the emitter faces the observer
    // head-on (cosAng = +1); p_r = +2 gives cosAng = -1 (receding).
    // ════════════════════════════════════════════════════════════════════════
    {
        const double pos[4]     = {0.0, 10.0, 0.0, 0.0};
        const double toward[4]  = {-1.0, -2.0, 0.0, 0.0};
        const double away[4]    = {-1.0,  2.0, 0.0, 0.0};

        // β = 0 must return exactly 1 — the unbeamed jet (spec scenario
        // "beaming asymmetry: β = 0 symmetric").
        const emission::JetBeam unbeamed{0.0, 0.7};
        CHECK(emission::jetBeamFactor(pos, toward, 0.5, unbeamed) == 1.0);
        CHECK(emission::jetBeamFactor(pos, away,   0.5, unbeamed) == 1.0);

        // Head-on worked example:  γ = 1/√(1-0.6²) = 1.25
        //                          δ = 1/(1.25 × (1-0.6)) = 2
        //                          gain = 2^3.7 = 12.99603834169977
        const emission::JetBeam beam{0.6, 0.7};
        CHECK_CLOSE(emission::jetBeamFactor(pos, toward, 0.5, beam),
                    12.99603834169977, 1e-7);

        // Same geometry with the march reversed — the emitter → observer sign
        // trap (ADR-0004 / CONTEXT §4.8):  δ = 1/(1.25 × (1+0.6)) = 0.5
        //                                 gain = 0.5^3.7 = 0.07694652583405726
        CHECK_CLOSE(emission::jetBeamFactor(pos, away, 0.5, beam),
                    0.07694652583405726, 1e-7);

        // Monotone in |β|: the approaching lobe keeps brightening, the
        // receding lobe keeps dimming, and both start from 1 at β = 0.
        double rising = 1.0;
        double falling = 1.0;
        bool ok = true;
        for (double b : {0.1, 0.5, 0.9}) {
            const emission::JetBeam param{b, 0.7};
            const double up   = emission::jetBeamFactor(pos, toward, 0.5, param);
            const double down = emission::jetBeamFactor(pos, away,   0.5, param);
            if (!(up > rising))         ok = false;
            if (!(down < falling))      ok = false;
            if (!(up > 1.0))            ok = false;
            if (!(down < 1.0))          ok = false;
            rising  = up;
            falling = down;
        }
        CHECK(ok);

        // Non-negativity across a sweep of geometries.
        bool finite = true;
        for (double th = 0.05; th < kPi; th += 0.31) {
            const double p[4] = {0.0, 12.0, th, 1.0};
            const double m[4] = {-1.0, -1.5, 0.3, 0.7};
            const double g = emission::jetBeamFactor(p, m, 0.6, {0.95, 0.7});
            if (!(g >= 0.0) || !std::isfinite(g)) finite = false;
        }
        CHECK(finite);
    }

    // ════════════════════════════════════════════════════════════════════════
    // Jet density — transverse Gaussian × bounded axial power law × soft top
    // (ADR-0003; CONTEXT §5.8 / §2 "Relativistic jet").
    // ════════════════════════════════════════════════════════════════════════
    {
        // On the axis (θ = 0 ⇒ ρ = 0 ⇒ gauss = 1, and z = r < 0.8·max_height
        // ⇒ top = 1) only the axial power law survives:
        //   j(z) = (1 + z/z_falloff)^(-axial_falloff)
        //   z = 10 → 2^-1.5  = 1/(2√2) = 0.3535533905932738
        //   z = 20 → 3^-1.5           = 0.19245008972987526
        //   z = 30 → 4^-1.5           = 0.125
        const double j10 = jetDensity(10.0, 0.0);
        const double j20 = jetDensity(20.0, 0.0);
        const double j30 = jetDensity(30.0, 0.0);
        CHECK_CLOSE(j10, 0.3535533905932738, 1e-12);
        CHECK_CLOSE(j20, 0.19245008972987526, 1e-12);
        CHECK_CLOSE(j30, 0.125, 1e-12);
        CHECK(j10 > j20 && j20 > j30);   // falloff is monotone

        // Axial cut: the soft top has fully faded at |z| = max_height.
        CHECK(jetDensity(40.0, 0.0) == 0.0);
        CHECK(jetDensity(45.0, 0.0) == 0.0);

        // Radial cut: the enclosing sphere bound rejects far samples.
        CHECK(emission::jetDensity(kBigBound, 0.0, kBigBound, kJet) == 0.0);
        CHECK(jetDensity(1000.0, 0.0) == 0.0);

        // Transverse cut at θ = π/2 (z = 0 ⇒ ρ_width = base_radius = 3.2):
        //   limit² = (3σ·ρ_width)² = (3 × 0.4 × 3.2)² = 14.7456
        //   r = 10 ⇒ ρ² = 100 ≥ 14.7456 → rejected
        CHECK(jetDensity(10.0, kHalfPi) == 0.0);

        // Transverse Gaussian at z = 0:  gauss(r) = exp(-r² / (2σ²·ρ_width²))
        //                                          = exp(-r² / 3.2768)
        //   r = 1 → 0.7369938133110109
        //   r = 3 → 0.06414673848332593
        // Just inside the cut (r = 3.7) the term is still positive.
        CHECK_CLOSE(jetDensity(1.0, kHalfPi), 0.7369938133110109, 1e-12);
        CHECK_CLOSE(jetDensity(3.0, kHalfPi), 0.06414673848332593, 1e-12);
        CHECK(jetDensity(3.0, kHalfPi) < jetDensity(1.0, kHalfPi));
        CHECK(jetDensity(3.7, kHalfPi) > 0.0);
        CHECK(jetDensity(3.7, kHalfPi) < jetDensity(3.0, kHalfPi));

        // No central blow-up: on the axis at the base the axial term is 1 and
        // ρ = 0, so the density is finite and exactly 1.
        const double centre = jetDensity(0.0, kHalfPi);
        CHECK(std::isfinite(centre));
        CHECK_CLOSE(centre, 1.0, 1e-12);

        // Non-negativity and finiteness over a broad sweep.
        bool clean = true;
        for (double r = 0.5; r < 60.0; r += 1.25) {
            for (double th = 0.02; th < kPi; th += 0.19) {
                const double j = jetDensity(r, th);
                if (!(j >= 0.0) || !std::isfinite(j)) clean = false;
            }
        }
        CHECK(clean);
    }

    // ════════════════════════════════════════════════════════════════════════
    // Disk emission — Novikov-Thorne-ish r^-3/4 temperature, gravitational
    // redshift √f, Keplerian Doppler³ (CONTEXT §4.6, §5.8).
    // ════════════════════════════════════════════════════════════════════════
    {
        const double rs = 2.0, rIn = 6.0, rOut = 12.0;

        // Face-on (inclination 0) kills the Doppler term entirely, so this
        // pins temperature × redshift at the inner edge:
        //   T(6)   = max(1-√1, 0.001)^0.25 = 0.001^0.25 = 10^-0.75
        //                                          = 0.1778279410038923
        //   √f(6)  = √(1 - 2/6) = √(2/3)   = 0.816496580927726
        //   intensity = 0.1778279410038923 × 0.816496580927726
        //             = 0.14519590582309544   (below the 5.0 clamp)
        //   palette  = mix(deep-red, orange-red, smoothstep(0, 0.3, T))
        const emission::Rgb edge = emission::diskEmission(6.0, 0.0, rs, rIn, rOut, 1.0, 0.0);
        CHECK_CLOSE(edge.r, 0.11888230035849451, 1e-12);
        CHECK_CLOSE(edge.g, 0.04229019905047763, 1e-12);
        CHECK_CLOSE(edge.b, 0.010309413707973397, 1e-12);

        // r^-3/4 radial shape, observed as the red-channel ratio between two
        // radii a factor of 2 apart, far out on the disk where both the
        // (1-√(r_in/r))^0.25 correction and the palette move by <0.3%:
        //   pure r^-3/4 law  → 2^0.75 = 1.681792830507429
        //   actually observed → 1.6787600674611627   (0.18% off)
        // The band is 1% of the ideal: wide enough to absorb the correction,
        // narrow enough that a wrong exponent fails decisively — 0.5 would give
        // 1.4142 and 1.0 would give 2.0, both >15% away.
        const emission::Rgb far1 = emission::diskEmission(6000.0, 0.0, rs, rIn, rOut, 1.0, 0.0);
        const emission::Rgb far2 = emission::diskEmission(12000.0, 0.0, rs, rIn, rOut, 1.0, 0.0);
        const double shape = far1.r / far2.r;
        CHECK_CLOSE(shape, 1.681792830507429, 1.681792830507429 * 0.01);

        // Face-on: no line-of-sight velocity, so the result is exactly
        // independent of the azimuth.
        const emission::Rgb f0 = emission::diskEmission(12.0, 0.0, rs, rIn, rOut, 1.0, 0.0);
        const emission::Rgb f1 = emission::diskEmission(12.0, 2.5, rs, rIn, rOut, 1.0, 0.0);
        CHECK(f0.r == f1.r && f0.g == f1.g && f0.b == f1.b);

        // Edge-on: the approaching side is Doppler boosted and the receding
        // side dimmed.  At r = 12, v_orb = √(rs/2r) = √(1/6) = 0.4082482905
        //   δ_appr = 1/(1-0.4082482905) = 1.68974...  → δ³ ≈ 4.823
        //   δ_recv = 1/(1+0.4082482905) = 0.71010...  → δ³ ≈ 0.358
        //   ratio of red channels = 5.946020129829382
        const emission::Rgb appr = emission::diskEmission(12.0, kHalfPi, rs, rIn, rOut, 1.0, 90.0);
        const emission::Rgb recv = emission::diskEmission(12.0, 3.0 * kHalfPi, rs, rIn, rOut, 1.0, 90.0);
        CHECK(appr.r > recv.r);
        CHECK_CLOSE(appr.r / recv.r, 5.946020129829382, 1e-6);

        // Non-negativity and finiteness across the disk's radial extent.
        bool clean = true;
        for (double r = rIn; r <= rOut; r += 0.37) {
            for (double phi = 0.0; phi < 2.0 * kPi; phi += 0.41) {
                const emission::Rgb c = emission::diskEmission(r, phi, rs, rIn, rOut, 1.0, 80.0);
                if (!(c.r >= 0.0 && c.g >= 0.0 && c.b >= 0.0)) clean = false;
                if (!std::isfinite(c.r) || !std::isfinite(c.g) || !std::isfinite(c.b)) clean = false;
            }
        }
        CHECK(clean);
    }

    return check::report("test_emission");
}
