#include "core/metric.h"

#include <cmath>

// ── Metric components with non-trivial bodies ─────────────────────────────────

double SchwarzschildMetric::g_phph(double r, double theta) const {
    double s = std::sin(theta);
    return r * r * s * s;
}

double SchwarzschildMetric::g_phph_inv(double r, double theta) const {
    double s = std::sin(theta);
    return 1.0 / (r * r * s * s);
}

// ── Geodesic equation RHS ─────────────────────────────────────────────────────
//
// Canonical Hamiltonian form:  H = ½ g^{μν} p_μ p_ν
//
//   dx^μ/dλ = ∂H/∂p_μ = g^{μν} p_ν          (raise index with inverse metric)
//   dp_μ/dλ = -∂H/∂x^μ = -½ ∂g^{αβ}/∂x^μ · p_α p_β
//
// Non-zero inverse metric components:
//   g^tt  = -1/f              f = 1 - rs/r
//   g^rr  =  f
//   g^θθ  =  1/r²
//   g^φφ  =  1/(r² sin²θ)
//
// Killing vectors ∂_t and ∂_φ → p_t and p_φ are conserved constants of motion.
//
// ∂g^tt/∂r  = -rs / (r² f²)
// ∂g^rr/∂r  =  rs / r²
// ∂g^θθ/∂r  = -2 / r³
// ∂g^φφ/∂r  = -2 / (r³ sin²θ)
// ∂g^φφ/∂θ  = -2 cosθ / (r² sin³θ)

SchwarzschildMetric::Derivatives
SchwarzschildMetric::geodesicRHS(const State& s) const {
    const double r     = s.x[1];
    const double theta = s.x[2];

    const double pt  = s.p[0];  // p_t  (conserved: -E)
    const double pr  = s.p[1];  // p_r
    const double pth = s.p[2];  // p_θ
    const double pph = s.p[3];  // p_φ  (conserved: L)

    const double f     = 1.0 - rs_ / r;
    const double f2    = f * f;
    const double r2    = r * r;
    const double r3    = r2 * r;
    const double sinth = std::sin(theta);
    const double costh = std::cos(theta);
    const double sin2  = sinth * sinth;
    const double sin3  = sin2  * sinth;

    Derivatives d;

    // dx^μ/dλ
    d.dx[0] = -pt  / f;              // dt/dλ
    d.dx[1] =  f   * pr;             // dr/dλ
    d.dx[2] =  pth / r2;             // dθ/dλ
    d.dx[3] =  pph / (r2 * sin2);    // dφ/dλ

    // dp_t/dλ = 0  (energy conserved)
    d.dp[0] = 0.0;

    // dp_r/dλ = -½ [ ∂g^tt/∂r · pt²  +  ∂g^rr/∂r · pr²
    //               + ∂g^θθ/∂r · pth² +  ∂g^φφ/∂r · pph² ]
    //
    // ∂g^tt/∂r = +rs/(r²f²)      (g^tt = -1/f)
    // ∂g^rr/∂r = +rs/r²          (g^rr = f)
    // ∂g^θθ/∂r = -2/r³           (g^θθ = 1/r²)
    // ∂g^φφ/∂r = -2/(r³sin²θ)    (g^φφ = 1/(r²sin²θ))
    d.dp[1] = -0.5 * (
        + rs_ / (r2 * f2) * pt  * pt
        + rs_ /  r2        * pr  * pr
        - 2.0 /  r3        * pth * pth
        - 2.0 / (r3 * sin2) * pph * pph
    );

    // dp_θ/dλ = -½ ∂g^φφ/∂θ · pph²  =  cosθ / (r² sin³θ) · pph²
    d.dp[2] = costh / (r2 * sin3) * pph * pph;

    // dp_φ/dλ = 0  (angular momentum conserved)
    d.dp[3] = 0.0;

    return d;
}

// ── Null-ray initialisation ───────────────────────────────────────────────────
//
// Null condition:  g^{μν} p_μ p_ν = 0
//
//   g^tt pt² + g^rr pr² + g^θθ pth² + g^φφ pph² = 0
//
//   pr² = -( g^tt pt² + g^θθ pth² + g^φφ pph² ) / g^rr
//        = -( (-1/f) pt² + (1/r²) pth² + (1/(r²sin²θ)) pph² ) / f

double SchwarzschildMetric::computeNullPr(double r, double theta,
                                          double pt, double pth, double pph,
                                          bool ingoing) const {
    const double f     = 1.0 - rs_ / r;
    const double r2    = r * r;
    const double sin2  = std::sin(theta) * std::sin(theta);

    double pr_sq = -(
        (-1.0 / f)         * pt  * pt  +
        ( 1.0 / r2)        * pth * pth +
        ( 1.0 / (r2*sin2)) * pph * pph
    ) / f;

    // Clamp tiny negative values caused by floating-point rounding (tangent ray)
    if (pr_sq < 0.0) pr_sq = 0.0;

    const double pr = std::sqrt(pr_sq);
    return ingoing ? -pr : pr;
}
