#ifndef METRIC_H
#define METRIC_H

#include <array>

// ═══════════════════════════════════════════════════════════════════════════════
// Schwarzschild Metric (non-rotating, uncharged black hole)
//
// In Boyer-Lindquist coordinates (t, r, θ, φ) with geometric units G = c = 1:
//
//   ds² = -(1 - rs/r) dt²  +  (1 - rs/r)⁻¹ dr²  +  r² dθ²  +  r² sin²θ dφ²
//
// where rs = 2M is the Schwarzschild radius.
//
// This class provides:
//   - Metric tensor components g_μν  (inline, called per pixel on GPU path)
//   - Inverse metric components g^μν (inline)
//   - Geodesic RHS for CPU RK4 integration  (defined in metric.cpp)
//   - Null-ray initialisation utility        (defined in metric.cpp)
//
// Coordinates:  x[0]=t, x[1]=r, x[2]=θ, x[3]=φ
// Momenta:      p[0]=pt, p[1]=pr, p[2]=pθ, p[3]=pφ
// ═══════════════════════════════════════════════════════════════════════════════

class SchwarzschildMetric {
public:
    explicit SchwarzschildMetric(double mass = 1.0)
        : M_(mass), rs_(2.0 * mass) {}

    // ── Basic parameters ──────────────────────────────────────────────────────
    double mass()                const { return M_; }
    double schwarzschildRadius() const { return rs_; }
    double eventHorizon()        const { return rs_; }
    double photonSphere()        const { return 1.5 * rs_; }  // r = 3M
    double iscoRadius()          const { return 3.0 * rs_; }  // r = 6M (prograde ISCO)

    // ── Metric components g_μν  (diagonal; off-diagonals = 0) ────────────────
    double g_tt  (double r)                const { return -(1.0 - rs_ / r); }
    double g_rr  (double r)                const { return 1.0 / (1.0 - rs_ / r); }
    double g_thth(double r)                const { return r * r; }
    double g_phph(double r, double theta)  const;

    // ── Inverse metric components g^μν ────────────────────────────────────────
    double g_tt_inv  (double r)               const { return -1.0 / (1.0 - rs_ / r); }
    double g_rr_inv  (double r)               const { return 1.0 - rs_ / r; }
    double g_thth_inv(double r)               const { return 1.0 / (r * r); }
    double g_phph_inv(double r, double theta) const;

    // ── State / Derivatives types for the geodesic integrator ─────────────────
    struct State {
        std::array<double, 4> x;  // coordinates: t, r, θ, φ
        std::array<double, 4> p;  // covariant momenta: p_t, p_r, p_θ, p_φ
    };

    struct Derivatives {
        std::array<double, 4> dx;  // dx^μ/dλ
        std::array<double, 4> dp;  // dp_μ/dλ
    };

    // ── Geodesic equation RHS (implemented in metric.cpp) ─────────────────────
    // Uses the canonical Hamiltonian form:
    //   dx^μ/dλ = g^{μν} p_ν
    //   dp_μ/dλ = -½ ∂g^{αβ}/∂x^μ · p_α p_β
    Derivatives geodesicRHS(const State& s) const;

    // ── Ray termination helpers ───────────────────────────────────────────────
    // Use a small buffer (1 + ε) above rs because Boyer-Lindquist coordinates
    // have a coordinate singularity at r = rs — the integrator can never
    // numerically reach r = rs exactly.
    static constexpr double HORIZON_EPSILON = 0.01;

    bool insideHorizon(double r) const {
        return r <= rs_ * (1.0 + HORIZON_EPSILON);
    }
    bool escaped(double r, double abort_dist) const { return r > abort_dist; }

    // ── Null-ray initialisation (implemented in metric.cpp) ───────────────────
    // Solve g^{μν} p_μ p_ν = 0 for p_r given (p_t, p_θ, p_φ).
    // ingoing = true  → p_r < 0  (ray travels toward the black hole)
    double computeNullPr(double r, double theta,
                         double pt, double pth, double pph,
                         bool ingoing = true) const;

private:
    double M_;   // black hole mass
    double rs_;  // Schwarzschild radius = 2M
};

#endif // METRIC_H
