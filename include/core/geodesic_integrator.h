#ifndef GEODESIC_INTEGRATOR_H
#define GEODESIC_INTEGRATOR_H

#include "core/metric.h"

// ═══════════════════════════════════════════════════════════════════════════════
// CPU Geodesic Integrator — RK4 with adaptive step size
//
// Integrates the 8 coupled ODEs (4 coordinates + 4 covariant momenta) along
// the affine parameter λ using the classical 4th-order Runge-Kutta method.
//
// Adaptive step control:
//   The step size is scaled down near the event horizon where the metric
//   components diverge (Boyer-Lindquist coordinate singularity at r = rs).
//   h_eff = h_base · min(1, (r - rs) / rs)
//   This keeps the integrator stable without switching coordinate charts.
//
// Termination conditions:
//   - CAPTURED:  ray falls inside the horizon buffer  (r ≤ rs·(1+ε))
//   - ESCAPED:   ray reaches the abort distance       (r > r_abort)
//   - MAX_STEPS: integration step limit exceeded
//   - ACTIVE:    still propagating (intermediate state)
// ═══════════════════════════════════════════════════════════════════════════════

enum class RayStatus {
    ACTIVE,
    CAPTURED,    // r ≤ rs·(1+ε)  → fell into the black hole
    ESCAPED,     // r > abort_distance → reached "infinity"
    MAX_STEPS    // step budget exhausted without termination
};

// Result returned after tracing a single ray
struct TraceResult {
    RayStatus status;
    SchwarzschildMetric::State finalState;
    int steps;           // number of RK4 steps taken
    double lambda;       // final affine parameter value
};

class GeodesicIntegrator {
public:
    GeodesicIntegrator(const SchwarzschildMetric& metric,
                       double stepSize,
                       int    maxSteps,
                       double abortDistance);

    // Trace a single null geodesic from the given initial state.
    TraceResult trace(SchwarzschildMetric::State s0) const;

    // Setters for runtime parameter tuning (e.g. from ImGui)
    void setStepSize(double h)        { h_    = h; }
    void setMaxSteps(int n)           { nmax_ = n; }
    void setAbortDistance(double d)    { rmax_ = d; }

private:
    const SchwarzschildMetric& metric_;
    double h_;       // base affine-parameter step size (Δλ)
    int    nmax_;    // maximum number of integration steps
    double rmax_;    // abort distance (escape threshold)

    // Compute effective step size scaled by proximity to horizon
    double adaptiveStep(double r) const;

    // One RK4 step: advance state by Δλ = h
    SchwarzschildMetric::State rk4Step(const SchwarzschildMetric::State& s,
                                       double h) const;

    // Check termination after each step
    RayStatus checkTermination(const SchwarzschildMetric::State& s) const;
};

#endif // GEODESIC_INTEGRATOR_H
