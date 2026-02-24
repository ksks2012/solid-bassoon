#include "core/geodesic_integrator.h"
#include <algorithm>  // std::min, std::max
#include <cmath>      // std::abs

GeodesicIntegrator::GeodesicIntegrator(const SchwarzschildMetric& metric,
                                       double stepSize,
                                       int    maxSteps,
                                       double abortDistance)
    : metric_(metric)
    , h_(stepSize)
    , nmax_(maxSteps)
    , rmax_(abortDistance)
{}

// ── Adaptive step size ────────────────────────────────────────────────────────
//
// Near the event horizon (r → rs), the Boyer-Lindquist metric components
// diverge: g_rr = 1/f → ∞ where f = 1 - rs/r.  This causes the momentum
// derivatives (dp_r/dλ in particular) to blow up, destabilising fixed-step
// RK4.  We scale the step size down proportionally to (r - rs) so that the
// integrator takes smaller steps where the metric changes rapidly.
//
//   h_eff = h_base · clamp( (r - rs) / (k · rs),  h_min_ratio,  1.0 )
//
// where k controls how close to the horizon we start shrinking (k = 2 means
// we begin reducing the step at r = 3·rs = photon sphere, which is the right
// ballpark for numerical safety), and h_min_ratio prevents the step from
// becoming negligibly small (the horizon epsilon will catch it).
//
// For large r the scaling factor is 1.0 and we use the full base step.

double GeodesicIntegrator::adaptiveStep(double r) const {
    const double rs = metric_.schwarzschildRadius();

    // Distance above the horizon
    const double dr = r - rs;

    // Scale factor: linearly shrinks as r approaches rs
    // Start shrinking at r = rs + k·rs = rs·(1+k), i.e. k=2 → r=3rs (photon sphere)
    constexpr double k = 2.0;
    constexpr double h_min_ratio = 0.001;  // never smaller than 0.1% of base step

    double scale = dr / (k * rs);
    scale = std::max(scale, h_min_ratio);
    scale = std::min(scale, 1.0);

    return h_ * scale;
}

// ── RK4 single step ──────────────────────────────────────────────────────────
//
// State vector  y = (x[4], p[4])  with  dy/dλ = f(y)
//
//   k1 = f(y)
//   k2 = f(y + h/2 · k1)
//   k3 = f(y + h/2 · k2)
//   k4 = f(y + h · k3)
//   y_new = y + h/6 · (k1 + 2·k2 + 2·k3 + k4)

SchwarzschildMetric::State
GeodesicIntegrator::rk4Step(const SchwarzschildMetric::State& s, double h) const {

    // Helper: apply  s_out = s_in + scale * deriv
    auto advance = [](const SchwarzschildMetric::State& base,
                      const SchwarzschildMetric::Derivatives& d,
                      double scale) -> SchwarzschildMetric::State
    {
        SchwarzschildMetric::State out;
        for (int i = 0; i < 4; ++i) {
            out.x[i] = base.x[i] + scale * d.dx[i];
            out.p[i] = base.p[i] + scale * d.dp[i];
        }
        return out;
    };

    const auto k1 = metric_.geodesicRHS(s);
    const auto s2 = advance(s, k1, 0.5 * h);

    const auto k2 = metric_.geodesicRHS(s2);
    const auto s3 = advance(s, k2, 0.5 * h);

    const auto k3 = metric_.geodesicRHS(s3);
    const auto s4 = advance(s, k3, h);

    const auto k4 = metric_.geodesicRHS(s4);

    // y_new = y + h/6 · (k1 + 2·k2 + 2·k3 + k4)
    SchwarzschildMetric::State result;
    for (int i = 0; i < 4; ++i) {
        result.x[i] = s.x[i] + (h / 6.0) *
            (k1.dx[i] + 2.0*k2.dx[i] + 2.0*k3.dx[i] + k4.dx[i]);
        result.p[i] = s.p[i] + (h / 6.0) *
            (k1.dp[i] + 2.0*k2.dp[i] + 2.0*k3.dp[i] + k4.dp[i]);
    }
    return result;
}

// ── Termination check ─────────────────────────────────────────────────────────

RayStatus GeodesicIntegrator::checkTermination(
    const SchwarzschildMetric::State& s) const
{
    const double r = s.x[1];

    if (metric_.insideHorizon(r))   return RayStatus::CAPTURED;
    if (metric_.escaped(r, rmax_))  return RayStatus::ESCAPED;

    return RayStatus::ACTIVE;
}

// ── Full trace ────────────────────────────────────────────────────────────────

TraceResult GeodesicIntegrator::trace(SchwarzschildMetric::State s0) const {
    TraceResult result;
    result.steps  = 0;
    result.lambda = 0.0;

    // Check initial state
    result.status = checkTermination(s0);
    if (result.status != RayStatus::ACTIVE) {
        result.finalState = s0;
        return result;
    }

    SchwarzschildMetric::State s = s0;

    for (int i = 0; i < nmax_; ++i) {
        // Compute effective step size based on current radial position
        double h_eff = adaptiveStep(s.x[1]);

        s = rk4Step(s, h_eff);
        result.steps  = i + 1;
        result.lambda += h_eff;

        result.status = checkTermination(s);
        if (result.status != RayStatus::ACTIVE) {
            result.finalState = s;
            return result;
        }
    }

    // Exhausted step budget
    result.status     = RayStatus::MAX_STEPS;
    result.finalState = s;
    return result;
}
