#include "check.h"
#include "support/physics_helpers.h"

#include "core/metric.h"

// Unit tests for SchwarzschildMetric (ticket 0009): metric constants, the
// inverse-metric identity, the geodesic RHS conservation laws, null-ray
// initialisation, and horizon/escape boundaries. Pure CPU — no GL context.

namespace {
constexpr double PI = testphys::PI;
using testphys::nullResidual;
} // namespace

int main() {
    // ── Constants ────────────────────────────────────────────────────────────
    SchwarzschildMetric m(1.0);
    CHECK(m.mass() == 1.0);
    CHECK_CLOSE(m.schwarzschildRadius(), 2.0, 1e-15);
    CHECK_CLOSE(m.eventHorizon(), 2.0, 1e-15);
    CHECK_CLOSE(m.photonSphere(), 3.0, 1e-15);  // 3M
    CHECK_CLOSE(m.iscoRadius(), 6.0, 1e-15);    // 6M

    SchwarzschildMetric m2(2.0);
    CHECK(m2.mass() == 2.0);
    CHECK_CLOSE(m2.schwarzschildRadius(), 4.0, 1e-15);
    CHECK_CLOSE(m2.photonSphere(), 6.0, 1e-15);
    CHECK_CLOSE(m2.iscoRadius(), 12.0, 1e-15);

    // ── Inverse identity g_μν · g^νλ = δ (diagonal → component product = 1) ──
    for (double r : {3.0, 6.0, 10.0, 100.0}) {
        for (double th : {0.5, PI / 2.0, 2.0}) {
            CHECK_CLOSE(m.g_tt(r)   * m.g_tt_inv(r),           1.0, 1e-9);
            CHECK_CLOSE(m.g_rr(r)   * m.g_rr_inv(r),           1.0, 1e-9);
            CHECK_CLOSE(m.g_thth(r) * m.g_thth_inv(r),         1.0, 1e-9);
            CHECK_CLOSE(m.g_phph(r, th) * m.g_phph_inv(r, th), 1.0, 1e-9);
        }
    }

    // g_tt vanishes at the horizon.
    CHECK_CLOSE(m.g_tt(m.schwarzschildRadius()), 0.0, 1e-15);

    // ── geodesicRHS conserves p_t and p_φ ────────────────────────────────────
    SchwarzschildMetric::State s;
    s.x = {0.0, 10.0, PI / 2.0, 0.0};
    s.p = {-1.0, -0.5, 0.3, 0.7};
    const auto d = m.geodesicRHS(s);
    CHECK(d.dp[0] == 0.0);  // p_t conserved
    CHECK(d.dp[3] == 0.0);  // p_φ conserved

    // A couple of RHS components against the analytic form.
    CHECK_CLOSE(d.dx[1], (1.0 - 2.0 / 10.0) * (-0.5), 1e-12);  // f·p_r
    CHECK_CLOSE(d.dx[3], 0.7 / 100.0, 1e-12);                  // p_φ/r²  (θ=π/2)
    // dp_r/dλ — the momentum kick that bends the ray; check the full analytic
    // value so a sign/coefficient slip in the r-equation is caught.
    {
        const double f = 1.0 - 2.0 / 10.0, f2 = f * f;
        const double expect = -0.5 * (
            + 2.0 / (100.0 * f2) * (-1.0) * (-1.0)   // rs/(r²f²)·p_t²
            + 2.0 / 100.0        * (-0.5) * (-0.5)   // rs/r²·p_r²
            - 2.0 / 1000.0       * 0.3 * 0.3         // -2/r³·p_θ²
            - 2.0 / 1000.0       * 0.7 * 0.7);       // -2/(r³sin²θ)·p_φ²
        CHECK_CLOSE(d.dp[1], expect, 1e-12);
    }
    CHECK_CLOSE(d.dp[2], 0.0, 1e-15);  // θ=π/2 ⇒ cosθ≈0

    // ── computeNullPr yields a null vector ───────────────────────────────────
    // Only for physically-real initial data: a real ingoing root exists iff
    // b² ≤ r²/f, where b = |p_φ| for an equatorial ray.
    const struct { double r, b; } cases[] = {
        {10.0, 1.0}, {10.0, 5.0}, {10.0, 10.0}, {50.0, 1.0}, {50.0, 20.0}
    };
    for (const auto& c : cases) {
        const double pt = -1.0, pth = 0.0, pph = c.b;

        SchwarzschildMetric::State in;
        in.x = {0.0, c.r, PI / 2.0, 0.0};
        in.p = {pt, 0.0, pth, pph};
        in.p[1] = m.computeNullPr(c.r, PI / 2.0, pt, pth, pph, true);

        CHECK(in.p[1] < 0.0);                         // ingoing
        CHECK_CLOSE(nullResidual(m, in), 0.0, 1e-9);  // null by construction

        const double prOut = m.computeNullPr(c.r, PI / 2.0, pt, pth, pph, false);
        CHECK(prOut > 0.0);                           // outgoing
        CHECK_CLOSE(prOut, -in.p[1], 1e-12);          // mirror image
    }

    // An over-large impact parameter has no real root at this radius; the
    // helper clamps p_r to 0 rather than returning NaN (a documented
    // approximation for near-tangent rays). b = 20 > √(r²/f) ≈ 11.18 at r = 10.
    CHECK(m.computeNullPr(10.0, PI / 2.0, -1.0, 0.0, 20.0, true) == 0.0);

    // Tangent ray (p_θ = p_φ = 0) is handled without a negative-sqrt.
    CHECK_CLOSE(m.computeNullPr(50.0, PI / 2.0, -1.0, 0.0, 0.0, true),
                -1.0 / (1.0 - 2.0 / 50.0), 1e-12);

    // ── Horizon / escape boundaries ──────────────────────────────────────────
    CHECK(m.insideHorizon(2.0));
    CHECK(m.insideHorizon(2.0199));   // just inside rs·(1+ε) = 2.02
    CHECK(!m.insideHorizon(2.0201));
    CHECK(!m.insideHorizon(3.0));

    CHECK(m.escaped(150.0, 100.0));
    CHECK(!m.escaped(100.0, 100.0));  // strict: r > abort
    CHECK(!m.escaped(50.0, 100.0));

    return check::report("metric");
}
