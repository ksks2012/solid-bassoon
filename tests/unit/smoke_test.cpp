#include "check.h"

#include "core/metric.h"

// Smoke test for ticket 0008: proves the tests target links `blackhole_core`
// and can call its public interface without a GL context. Expected values are
// known-good literals (rs = 2M, photon sphere 3M, ISCO 6M), not recomputed.
int main() {
    SchwarzschildMetric metric(1.0);

    CHECK(metric.mass() == 1.0);
    CHECK(metric.schwarzschildRadius() == 2.0);
    CHECK(metric.photonSphere() == 3.0);
    CHECK(metric.iscoRadius() == 6.0);

    return check::report("smoke");
}
