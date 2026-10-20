// ═══════════════════════════════════════════════════════════════════════════════
// Physics kernels — shared pure functions (ticket 0012)
//
// Every function below is PURE: it reads no uniforms and touches no global
// state.  Callers pass the parameters explicitly.  This is what makes the
// kernels testable from `shaders/probe.comp` (which has a different uniform
// block) and keeps `raytrace.comp` as the only place uniforms are interpreted.
//
// There is no `#version` line here: this file is textually expanded by
// `loadShaderSource()` in src/rendering/ray_tracer.cpp at each `#include`, so
// the includer's `#version` stays the literal first line of the program.
//
// The CPU reference for all four kernels lives in
// `include/core/emission_reference.h` (spec/0002 seam S1); `tests/gpu/probe_check.py`
// cross-validates the two against each other (seam S3).
// ═══════════════════════════════════════════════════════════════════════════════

const float PI = 3.14159265358979323846;

// ── Parameter blocks ──────────────────────────────────────────────────────────
// Plain structs (not uniform blocks): constructed at the call site from the
// caller's uniforms so the kernels stay uniform-free.

// Transverse + axial shape of the relativistic jet (ADR-0003).
struct JetShape {
    float base_radius;       // cone foot radius at z = 0
    float opening_angle_deg; // half-opening angle (degrees)
    float profile_exponent;  // 1 = cone, 0.5 = parabola
    float max_height;        // axial extent; emission fades to 0 across it
    float radial_sigma;      // transverse Gaussian width
    float z_falloff;         // axial falloff length scale
    float axial_falloff;     // axial power-law exponent
};

// Constant bulk velocity producing Doppler beaming (ADR-0004).
struct JetBeam {
    float bulk_velocity;   // β
    float spectral_index;  // α in δ^(3+α)
};

// ── Accretion disk emission ───────────────────────────────────────────────────
//
// Temperature profile (simplified Novikov-Thorne):
//   T(r) ∝ r^{-3/4} · [1 - √(r_in/r)]^{1/4}
//
// Maps to a blackbody-inspired colour palette:
//   hot (near ISCO)  → blue-white   (T ~ 1.0)
//   warm (mid disk)  → yellow-white (T ~ 0.5)
//   cool (outer edge) → red-orange   (T ~ 0.1)
//   T < 0.1          → deep red
//
// Gravitational redshift dims the emission by √f(r).
// Doppler boost from Keplerian orbital motion shifts the colour for
// gas moving toward/away from the observer; the observer's inclination
// arrives as a parameter rather than being read from a uniform.
vec3 diskEmission(float r, float phi, float rs, float r_in, float r_out,
                  float temp_scale, float inclination_deg) {
    // Novikov-Thorne temperature profile (simplified)
    float ratio = r_in / r;
    float T = pow(r_in / r, 0.75) * pow(max(1.0 - sqrt(ratio), 0.001), 0.25);
    T *= temp_scale;

    // Blackbody colour palette (normalised T from 0 to ~1)
    vec3 hotColor  = vec3(0.8, 0.85, 1.0);    // blue-white
    vec3 warmColor = vec3(1.0, 0.9, 0.6);     // yellow-white
    vec3 coolColor = vec3(1.0, 0.4, 0.1);     // orange-red
    vec3 coldColor = vec3(0.5, 0.1, 0.02);    // deep red

    vec3 diskColor;
    if (T > 0.6) {
        diskColor = mix(warmColor, hotColor, smoothstep(0.6, 1.0, T));
    } else if (T > 0.3) {
        diskColor = mix(coolColor, warmColor, smoothstep(0.3, 0.6, T));
    } else {
        diskColor = mix(coldColor, coolColor, smoothstep(0.0, 0.3, T));
    }

    // Gravitational redshift: emitted light is dimmed by √f at the emission point
    float f = max(1.0 - rs / r, 0.01);
    float redshift = sqrt(f);

    // Keplerian Doppler boost (for inclined views):
    // Orbital velocity v_orb = √(M / r) = √(rs / (2r)) in geometric units.
    // The line-of-sight component depends on azimuthal angle φ and inclination.
    // For a prograde disk, gas at angle φ moves in the +φ direction.
    // Doppler factor δ ≈ 1 / (1 - v_los), where v_los = v_orb · sin(φ) · sin(θ_obs).
    float v_orb = sqrt(rs / (2.0 * r));
    float theta_obs = inclination_deg * PI / 180.0;
    float v_los = v_orb * sin(phi) * sin(theta_obs);
    float doppler = 1.0 / (1.0 - clamp(v_los, -0.9, 0.9));

    // Combined intensity: temperature brightness × redshift × Doppler^3
    // (Doppler^3 for relativistic beaming of bolometric flux)
    float intensity = T * redshift * doppler * doppler * doppler;
    intensity = clamp(intensity, 0.0, 5.0);

    return diskColor * intensity;
}

// ── Relativistic jet density ──────────────────────────────────────────────────
// Axisymmetric solid cone around the symmetry axis.  The emission volume is
// described in the (z, ρ) plane, with z = r·cosθ along the axis and
// ρ = r·sinθ the cylindrical radius:
//
//   rho_width(z) = base_radius + |z|^p · tan(opening_angle)   (p = profile_exponent)
//   gauss(ρ, z)  = exp( -(ρ / rho_width)² / (2·σ²) )           (transverse Gaussian)
//   axial(z)     = (1 + |z|/z_falloff)^(-axial_falloff)        (bounded at z = 0)
//   top(z)       = smoothstep(max_height, 0.8·max_height, |z|)
//   j(ρ, z)      = gauss · axial · top
//
// `p = 1` gives the cone; lowering it toward 0.5 turns the cone into a parabola
// by changing a single parameter.  The axial term is finite (= 1) on the axis
// at z = 0, so there is no central blow-up or hot spot, and the soft top fades
// the emission to zero before `max_height`, so there is no hard cut-off.
//
// The whole kernel is skipped for rays outside the solid of revolution (a cheap
// bounding early-out).  Emission is optically thin and added to every terminal
// colour, so the jet never occludes the disk, stars or Einstein rings.
//
// `rBound` is the caller's enclosing-sphere radius (the first, cheapest reject).
float jetDensity(float r, float theta, float rBound, JetShape s) {
    // ── Bounding early-out (radial) ───────────────────────────────────────────
    // The solid of revolution fits inside a sphere of radius rBound, so rays
    // beyond it can never sample the jet.  Cheapest possible reject: one compare
    // before the cos, catching the long outward legs of escaping rays.
    if (r >= rBound) return 0.0;

    float z     = r * cos(theta);   // only cos is needed: ρ² = r² − z²
    float az    = abs(z);           // baryon-symmetric bipolar jet

    // ── Bounding early-out (axial): outside the finite extent, nothing to do ──
    // Cheap: rejects far rays after a single cos, before any profile work.
    if (az >= s.max_height) return 0.0;

    float exponent = max(s.profile_exponent, 0.05);   // protected shape exponent

    // Cone by default: p = 1 needs no pow; any other p turns it into a parabola.
    float axial_shape = az;
    if (abs(exponent - 1.0) > 1e-4) axial_shape = pow(az, exponent);

    float opening   = radians(s.opening_angle_deg);
    float rho_width = s.base_radius + axial_shape * tan(opening);
    float sigma     = s.radial_sigma;

    // ── Bounding early-out (transverse) ──────────────────────────────────────
    // Compare ρ² = r² − z² against (3σ·ρ_width)² — avoids sqrt/sin entirely.
    float rho2 = r * r - z * z;
    float lim  = 3.0 * sigma * rho_width;
    if (rho2 >= lim * lim) return 0.0;

    // Transverse Gaussian — solid volume, core-bright and rim-soft.
    float gauss = exp(-rho2 / (2.0 * sigma * sigma * rho_width * rho_width));

    // Bounded axial power-law: brightest at the base, declining outward.
    float axial = pow(1.0 + az / s.z_falloff, -s.axial_falloff);

    // Soft top: fade between 0.8·max_height and max_height → no hard cut-off.
    // Written the conformant way around (edge0 < edge1, then inverted), because
    // GLSL leaves smoothstep undefined for edge0 >= edge1.
    axial *= 1.0 - smoothstep(s.max_height * 0.8, s.max_height, az);

    return gauss * axial;
}

// ── Relativistic Doppler beaming ──────────────────────────────────────────────
// A single constant bulk velocity v = sign(z)·β·ẑ makes the lobe moving toward
// the observer brighter and the receding lobe dimmer (ticket 0006).  The Doppler
// factor is assembled in the static local frame from the covariant momenta the
// ray already carries (normalised to p_t = -1, i.e. E = 1):
//
//   n̂ = ( f·p_r, √f·p_θ/r, √f·p_φ/(r·sinθ) )   (observer → scene = march dir)
//   ẑ = cosθ·ê_r − sinθ·ê_θ                     (jet axis, local frame)
//   δ = 1 / [ γ·(1 − β·(v̂·(−n̂))) ],  γ = 1/√(1−β²)
//   gain = δ^(3 + α)                            (α = spectral index)
//
// Because the ray is traced backwards, the emitter → observer direction is the
// reverse of the march direction, hence the −n̂ (the classic sign trap).  β = 0
// returns 1, reproducing the unbeamed jet.  `f` is the caller's metric factor.
float jetBeamFactor(vec4 pos, vec4 mom, float f, JetBeam b) {
    float beta = clamp(b.bulk_velocity, -0.999, 0.999);
    if (abs(beta) < 1e-5) return 1.0;              // no bulk motion → unbeamed

    float r     = pos.y;
    float theta = pos.z;
    float sinT  = max(sin(theta), 1e-4);
    float cosT  = cos(theta);
    float sqrtF = sqrt(f);

    // Local photon direction of the backward march (observer → scene).
    vec3 n = vec3(f * mom.y,
                  sqrtF * mom.z / r,
                  sqrtF * mom.w / (r * sinT));

    // Bipolar bulk direction v̂ = sign(z)·ẑ, with sign(z) = sign(cosθ).
    float sz   = (cosT >= 0.0) ? 1.0 : -1.0;
    vec3  vhat = sz * vec3(cosT, -sinT, 0.0);

    // Emitter → observer is the reverse of the backward march, hence −n̂.
    // Clamp guards against any |n̂| drift so δ^… never sees a negative base.
    float cosAng = clamp(dot(vhat, -n), -1.0, 1.0);
    float gamma  = 1.0 / sqrt(1.0 - beta * beta);
    float delta  = 1.0 / (gamma * (1.0 - beta * cosAng));
    return pow(delta, 3.0 + b.spectral_index);
}

// ── Global tone mapping (display transform) ───────────────────────────────────
// Maps linear HDR radiance to displayable LDR.  With the defaults
// (exposure = 1, gamma = 1) the normal range is left untouched — visually
// identical to a plain clamp — while HDR pixels (photon-ring glow, point
// source) roll off smoothly instead of clipping to flat white.
//   exposure  → linear scale
//   soft knee → identity below `knee`, compresses highlights above it,
//               approaching `knee + width` (never reaches it)
//   gamma     → optional display encoding (default 1.0 = identity)
vec3 applyToneMap(vec3 linear, float exposure, float gamma) {
    vec3 c = max(linear * exposure, 0.0);
    const float knee  = 0.8;
    const float width = 0.2;
    vec3 over = max(c - knee, 0.0);
    vec3 mapped = min(c, knee) + over * width / (width + over);
    mapped = pow(mapped, vec3(1.0 / max(gamma, 1e-3)));
    return mapped;
}
