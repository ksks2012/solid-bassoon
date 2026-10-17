#ifndef BLACKHOLE_TESTS_SUPPORT_PHYSICS_HELPERS_H
#define BLACKHOLE_TESTS_SUPPORT_PHYSICS_HELPERS_H

#include "core/metric.h"

// Shared helpers for the CPU physics tests (ticket 0009). Keeping them in one
// place avoids the same null-condition algebra drifting between test files.

namespace testphys {

constexpr double PI = 3.14159265358979323846;

// g^{μν} p_μ p_ν — identically zero for a null geodesic.
inline double nullResidual(const SchwarzschildMetric& m,
                           const SchwarzschildMetric::State& s) {
    const double r = s.x[1];
    const double th = s.x[2];
    return m.g_tt_inv(r) * s.p[0] * s.p[0]
         + m.g_rr_inv(r) * s.p[1] * s.p[1]
         + m.g_thth_inv(r) * s.p[2] * s.p[2]
         + m.g_phph_inv(r, th) * s.p[3] * s.p[3];
}

} // namespace testphys

#endif // BLACKHOLE_TESTS_SUPPORT_PHYSICS_HELPERS_H
