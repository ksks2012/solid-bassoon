#include "core/emission_reference.h"

#include <algorithm>
#include <cmath>

namespace emission {
namespace {

constexpr double kPi = 3.14159265358979323846;

double clamp(double v, double lo, double hi) {
    return std::min(std::max(v, lo), hi);
}

// GLSL smoothstep: t = clamp((x-e0)/(e1-e0), 0, 1); t·t·(3-2t).
double smoothstep(double e0, double e1, double x) {
    const double t = clamp((x - e0) / (e1 - e0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// GLSL mix(x, y, a) = x·(1-a) + y·a.
double mix(double x, double y, double a) {
    return x * (1.0 - a) + y * a;
}

// Palette that maps a disk temperature to a colour (CONTEXT §5.8): three
// temperature regimes, each a linear blend over its own smoothstep window.
void colourForTemperature(double T, double& outR, double& outG, double& outB) {
    if (T > 0.6) {
        const double s = smoothstep(0.6, 1.0, T);
        outR = mix(1.0, 0.8, s);  // hot   → warm
        outG = mix(0.9, 0.85, s);
        outB = mix(0.6, 1.0, s);
    } else if (T > 0.3) {
        const double s = smoothstep(0.3, 0.6, T);
        outR = mix(1.0, 1.0, s);  // orange-red → hot
        outG = mix(0.4, 0.9, s);
        outB = mix(0.1, 0.6, s);
    } else {
        const double s = smoothstep(0.0, 0.3, T);
        outR = mix(0.5, 1.0, s);  // deep-red → orange-red
        outG = mix(0.1, 0.4, s);
        outB = mix(0.02, 0.1, s);
    }
}

// ADR-0001 soft knee: identity below `knee`, asymptotic at `knee + width`.
double kneeCurve(double c) {
    constexpr double kKnee = 0.8;
    constexpr double kWidth = 0.2;
    const double over = std::max(c - kKnee, 0.0);
    return std::min(c, kKnee) + over * kWidth / (kWidth + over);
}

} // namespace

double jetDensity(double r, double theta, double rBound, const JetShape& s) {
    if (r >= rBound) return 0.0;

    const double z      = r * std::cos(theta);
    const double az     = std::fabs(z);
    if (az >= s.max_height) return 0.0;

    const double exponent = std::max(s.profile_exponent, 0.05);
    const double axialShape =
        std::fabs(exponent - 1.0) > 1e-4 ? std::pow(az, exponent) : az;

    const double rhoWidth = s.base_radius +
                            axialShape * std::tan(s.opening_angle_deg * kPi / 180.0);
    const double sigma = s.radial_sigma;

    const double rho2 = r * r - z * z;
    const double lim  = 3.0 * sigma * rhoWidth;
    if (rho2 >= lim * lim) return 0.0;

    const double gauss = std::exp(-rho2 / (2.0 * sigma * sigma * rhoWidth * rhoWidth));

    double axial = std::pow(1.0 + az / s.z_falloff, -s.axial_falloff);
    axial *= 1.0 - smoothstep(0.8 * s.max_height, s.max_height, az);

    return gauss * axial;
}

double jetBeamFactor(const double pos[4], const double mom[4], double f,
                     const JetBeam& beam) {
    const double beta = clamp(beam.bulk_velocity, -0.999, 0.999);
    if (std::fabs(beta) < 1e-5) return 1.0;

    const double r     = pos[1];
    const double theta = pos[2];

    // Observer direction: the polar axis (θ = 0) points at the observer.
    const double cosT = std::cos(theta);
    const double sinT = std::max(std::sin(theta), 1e-4);

    // Emission direction from the momentum: the photon travels along p̂, so the
    // surface faces the observer along -n̂ (ADR-0004 sign convention).
    const double nx = f * mom[1];
    const double ny = std::sqrt(f) * mom[2] / r;
    const double nz = std::sqrt(f) * mom[3] / (r * sinT);

    const double sz = cosT >= 0.0 ? 1.0 : -1.0;
    const double vx = sz * cosT;
    const double vy = -sz * sinT;

    // cos of the angle between the outward-facing normal and the observer.
    const double cosAng = clamp(-(vx * nx + vy * ny), -1.0, 1.0);

    const double gamma = 1.0 / std::sqrt(1.0 - beta * beta);
    const double delta = 1.0 / (gamma * (1.0 - beta * cosAng));

    return std::pow(delta, 3.0 + beam.spectral_index);
}

Rgb diskEmission(double r, double phi, double rs, double rIn, double rOut,
                 double tempScale, double inclinationDeg) {
    // Temperature profile: r^-3/4 with a hard zero inside the ISCO.
    const double ratio = rIn / r;
    double T = std::pow(ratio, 0.75) *
               std::pow(std::max(1.0 - std::sqrt(ratio), 0.001), 0.25);
    T *= tempScale;

    double cr, cg, cb;
    colourForTemperature(T, cr, cg, cb);

    const double f         = std::max(1.0 - rs / r, 0.01);
    const double redshift  = std::sqrt(f);
    const double vOrb      = std::sqrt(rs / (2.0 * r));
    const double thetaObs  = inclinationDeg * kPi / 180.0;
    const double vLos      = vOrb * std::sin(phi) * std::sin(thetaObs);
    const double doppler   = 1.0 / (1.0 - clamp(vLos, -0.9, 0.9));

    double intensity = T * redshift;
    intensity *= doppler;
    intensity *= doppler;
    intensity *= doppler;
    intensity = clamp(intensity, 0.0, 5.0);

    return {cr * intensity, cg * intensity, cb * intensity};
}

Rgb toneMap(const Rgb& linear, const ToneMap& params) {
    const double g = 1.0 / std::max(params.gamma, 1e-3);
    const auto   one = [&](double c) {
        return std::pow(kneeCurve(std::max(c * params.exposure, 0.0)), g);
    };
    return {one(linear.r), one(linear.g), one(linear.b)};
}

} // namespace emission
