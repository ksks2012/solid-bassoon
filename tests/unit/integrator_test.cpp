#include "check.h"
#include "support/physics_helpers.h"

#include "core/geodesic_integrator.h"
#include "core/metric.h"

#include <cmath>

// Unit tests for GeodesicIntegrator (ticket 0009): conservation laws, ray
// classification (CAPTURED / ESCAPED / MAX_STEPS), weak-field deflection,
// RK4 convergence order, and the adaptive-step policy. Pure CPU — no GL.

namespace {

constexpr double PI = testphys::PI;
using testphys::nullResidual;

// Equatorial null ray launched from radius r0 with impact parameter b = L/E.
SchwarzschildMetric::State equatorial(const SchwarzschildMetric& m,
                                      double r0, double b) {
    SchwarzschildMetric::State s;
    s.x = {0.0, r0, PI / 2.0, 0.0};
    s.p = {-1.0, 0.0, 0.0, b};  // p_t = -E = -1, p_φ = L = b
    s.p[1] = m.computeNullPr(r0, PI / 2.0, s.p[0], s.p[2], s.p[3], true);
    return s;
}

} // namespace

int main() {
    SchwarzschildMetric m(1.0);

    // ── Conservation: p_t and p_φ are Killing constants ──────────────────────
    {
        GeodesicIntegrator gi(m, 0.05, 1000000, 1000.0);
        const auto s0 = equatorial(m, 50.0, 6.0);
        const auto res = gi.trace(s0);
        CHECK(res.status == RayStatus::ESCAPED);
        CHECK_CLOSE(res.finalState.p[0], s0.p[0], 0.0);  // p_t unchanged
        CHECK_CLOSE(res.finalState.p[3], s0.p[3], 0.0);  // p_φ unchanged

        // Null condition drift stays small over the whole trace. Measured at
        // the endpoint, which bounds the accumulated RK4 null error; ~3e-13.
        CHECK_CLOSE(nullResidual(m, res.finalState), 0.0, 1e-12);
    }

    // ── Classification ───────────────────────────────────────────────────────
    // Radial infall (b = 0) is captured.
    {
        GeodesicIntegrator gi(m, 0.05, 1000000, 1000.0);
        const auto res = gi.trace(equatorial(m, 50.0, 0.0));
        CHECK(res.status == RayStatus::CAPTURED);
        CHECK(res.finalState.x[1] <= m.schwarzschildRadius() * 1.01 + 1e-9);
        CHECK(res.steps > 0);
    }
    // Large impact parameter (b = 20 > 3√3 M) escapes.
    {
        GeodesicIntegrator gi(m, 0.05, 1000000, 100.0);
        const auto res = gi.trace(equatorial(m, 50.0, 20.0));
        CHECK(res.status == RayStatus::ESCAPED);
        CHECK(res.finalState.x[1] > 100.0);
    }
    // A tiny budget exhausts before any termination.
    {
        GeodesicIntegrator gi(m, 0.05, 5, 100.0);
        const auto res = gi.trace(equatorial(m, 50.0, 20.0));
        CHECK(res.status == RayStatus::MAX_STEPS);
        CHECK(res.steps == 5);
    }
    // Initial state already inside the horizon → immediate capture. r = 2.01
    // is just inside rs·(1+ε) = 2.02 but keeps f > 0, so building the (unused)
    // initial momenta never divides by zero.
    {
        GeodesicIntegrator gi(m, 0.05, 1000, 100.0);
        const auto res = gi.trace(equatorial(m, 2.01, 20.0));
        CHECK(res.status == RayStatus::CAPTURED);
        CHECK(res.steps == 0);
    }
    // Initial state already beyond the abort distance → immediate escape.
    {
        GeodesicIntegrator gi(m, 0.05, 1000, 100.0);
        const auto res = gi.trace(equatorial(m, 200.0, 20.0));
        CHECK(res.status == RayStatus::ESCAPED);
        CHECK(res.steps == 0);
    }

    // Start and abort at the same large radius r0 so the sweep is symmetric.
    // Subtract the straight-line reference 2·acos(b/r0) to isolate the
    // gravitational deflection. The leading term is 4M/b; the O((M/b)²)
    // correction is ~5.9% at b = 50 M (measured 6.46%), so a 10% band admits
    // the neglected second-order term while still catching a coefficient slip.
    {
        const double b = 50.0, r0 = 200.0;
        GeodesicIntegrator gi(m, 0.05, 1000000, r0);
        const auto res = gi.trace(equatorial(m, r0, b));
        CHECK(res.status == RayStatus::ESCAPED);
        const double alpha = res.finalState.x[3] - 2.0 * std::acos(b / r0);
        CHECK_CLOSE(alpha, 4.0 / b, 0.10 * (4.0 / b));
    }

    // ── RK4 error scales ≈ h⁴ ────────────────────────────────────────────────
    //
    // Integrate to a fixed affine parameter Λ at shrinking step h and measure
    // successive-refinement differences e(h) = |φ(h) − φ(h/2)|. For a 4th-order
    // method e(h)/e(h/2) ≈ 16. The ray stays at r > 6 M, where adaptiveStep is
    // constant, isolating the RK4 order from the step policy.
    {
        const double r0 = 15.0, b = 10.0, lambda = 3.0;
        auto phiAt = [&](double h) {
            const int n = static_cast<int>(std::lround(lambda / h));
            GeodesicIntegrator gi(m, h, n, 1000.0);
            return gi.trace(equatorial(m, r0, b)).finalState.x[3];
        };
        const double e1 = std::fabs(phiAt(0.20) - phiAt(0.10));
        const double e2 = std::fabs(phiAt(0.10) - phiAt(0.05));
        const double e3 = std::fabs(phiAt(0.05) - phiAt(0.025));
        CHECK(e1 > 1e-12);              // well above round-off
        CHECK(e2 > 0.0);
        CHECK(e3 > 0.0);
        CHECK(e1 / e2 > 10.0 && e1 / e2 < 25.0);
        CHECK(e2 / e3 > 10.0 && e2 / e3 < 25.0);
    }

    // ── adaptiveStep: capped, floored, monotone ──────────────────────────────
    {
        const double h = 0.05;
        GeodesicIntegrator gi(m, h, 1000, 1000.0);

        // Capped at the base step far from the horizon (and from r ≥ 3·rs).
        CHECK_CLOSE(gi.adaptiveStep(1000.0), h, 1e-15);
        CHECK_CLOSE(gi.adaptiveStep(6.0), h, 1e-15);   // scale exactly 1
        CHECK(gi.adaptiveStep(1000.0) <= h + 1e-15);

        // Known linear value: scale = (r − rs)/(2·rs) for rs < r < 3·rs.
        CHECK_CLOSE(gi.adaptiveStep(4.0), 0.5 * h, 1e-15);  // (4−2)/4 = 0.5

        // Floored at 0.1% of the base step at the horizon.
        CHECK_CLOSE(gi.adaptiveStep(2.0), 0.001 * h, 1e-18);
        CHECK_CLOSE(gi.adaptiveStep(1.0), 0.001 * h, 1e-18);  // below rs too

        // Monotone non-decreasing in r.
        double prev = -1.0;
        for (double r = 2.0; r <= 10.0; r += 0.25) {
            const double v = gi.adaptiveStep(r);
            CHECK(v >= prev - 1e-15);
            prev = v;
        }
    }

    return check::report("integrator");
}
