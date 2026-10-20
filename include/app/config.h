#ifndef CONFIG_H
#define CONFIG_H

#include <nlohmann/json.hpp>
#include <string>
#include <stdexcept>

using json = nlohmann::json;

// Custom exception for configuration errors
class ConfigError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Blackhole parameters (Schwarzschild / Kerr)
struct BlackholeParams {
    double mass = 1.0;   // M  (geometric units, G = c = 1)
    double spin = 0.0;   // a  (Kerr spin parameter, 0 = Schwarzschild)
    double rs   = 2.0;   // Schwarzschild radius = 2M
};

// Observer camera
struct CameraParams {
    double distance    = 30.0;  // r_obs in units of M
    double inclination = 80.0;  // degrees from spin axis
    double azimuth     = 0.0;   // degrees
    double fov         = 60.0;  // field of view in degrees
};

// Accretion disk geometry
struct DiskParams {
    double inner_radius = 6.0;   // ISCO = 3·rs for Schwarzschild (rs = 2)
    double outer_radius = 12.0;
    double temperature  = 1.0;   // normalised
    bool   enabled      = true;  // on by default; headless regressions toggle it
};

// Ray-tracing render settings
struct RenderParams {
    int    width          = 1280;
    int    height         = 720;
    int    max_steps      = 1000;
    double step_size      = 0.05;
    double abort_distance = 100.0;
    double exposure       = 1.0;   // linear exposure multiplier
    double gamma          = 1.0;   // display gamma (1.0 = linear/identity)
};

// Relativistic jet (optically thin, additive) — off by default
struct JetConfig {
    bool   enabled          = false;  // opt-in
    double base_radius      = 3.2;    // cone foot radius at z = 0 (> photon sphere)
    double opening_angle    = 7.0;    // half-opening angle (degrees)
    double profile_exponent = 1.0;    // 1 = cone, 0.5 = parabola
    double max_height       = 40.0;   // axial extent of the jet
    double radial_sigma     = 0.4;    // transverse Gaussian width
    double z_falloff        = 10.0;   // axial falloff length scale
    double axial_falloff    = 1.5;    // axial power-law exponent
    double bulk_velocity    = 0.95;   // β (constant)
    double spectral_index   = 0.7;    // α in the beaming exponent δ^(3+α)
    double emissivity       = 1.0;    // master brightness multiplier
};

class Config {
public:
    Config() = default;
    void loadFromFile(const std::string& path);

    BlackholeParams blackhole;
    CameraParams    camera;
    DiskParams      disk;
    RenderParams    render;
    JetConfig       jet;
    int             logger_level = 1;  // 0=DEBUG 1=INFO 2=WARN 3=ERROR

private:
    void parseConfig(const json& j);
};

#endif // CONFIG_H
