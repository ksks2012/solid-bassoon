#ifndef ACCRETION_DISK_H
#define ACCRETION_DISK_H

// ═══════════════════════════════════════════════════════════════════════════════
// Thin Accretion Disk Model (Novikov-Thorne / Geometrically Thin)
//
// The disk lies in the equatorial plane (θ = π/2) and extends from an inner
// radius r_in (typically ISCO = 6M for Schwarzschild) to an outer radius r_out.
//
// During geodesic integration, we detect when a ray crosses the equatorial
// plane (θ passes through π/2).  At the crossing point we check whether the
// radial coordinate falls within [r_in, r_out]; if so, the ray "hits" the disk
// and we colour it according to a simplified temperature/emission model.
//
// ── Disk crossing detection ───────────────────────────────────────────────────
//
// Between two integration steps the colatitude changes from θ_prev to θ_curr.
// A crossing occurs when:
//     (θ_prev - π/2) * (θ_curr - π/2) < 0
//
// The crossing radius is found by linear interpolation:
//     t = (π/2 - θ_prev) / (θ_curr - θ_prev)
//     r_cross = r_prev + t * (r_curr - r_prev)
//
// If r_in ≤ r_cross ≤ r_out, the ray hits the disk.
//
// ── Emission model ────────────────────────────────────────────────────────────
//
// For a geometrically thin, optically thick disk:
//   - Temperature profile T(r) ∝ r^{-3/4} · [1 - √(r_in/r)]^{1/4}
//     (Shakura-Sunyaev / Page-Thorne, simplified)
//   - Colour is derived from a simplified blackbody palette:
//     hot (near ISCO) → blue-white,  cool (outer edge) → red-orange.
//   - Gravitational redshift: the emitted light is redshifted by √f(r)
//     where f(r) = 1 - rs/r (Schwarzschild factor at the emission point).
//   - Doppler shift from orbital velocity is included for inclined views.
//
// All physics is implemented in the compute shader (raytrace.comp);
// this header documents the model and provides constants for the CPU side.
// ═══════════════════════════════════════════════════════════════════════════════

namespace AccretionDisk {

// Default radii for Schwarzschild (M = 1, rs = 2)
constexpr double DEFAULT_INNER_RADIUS = 6.0;   // ISCO = 3·rs
constexpr double DEFAULT_OUTER_RADIUS = 20.0;   // arbitrary outer edge
constexpr double DEFAULT_TEMPERATURE  = 1.0;    // normalised peak temperature

// Keplerian orbital velocity in Schwarzschild spacetime (circular orbit):
//   v_φ = √(M / r) in geometric units
// Angular velocity Ω = √(M / r³)
// Used for Doppler shift calculation on the GPU.

} // namespace AccretionDisk

#endif // ACCRETION_DISK_H
