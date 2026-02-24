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
    double inner_radius = 3.0;   // ISCO for Schwarzschild
    double outer_radius = 12.0;
    double temperature  = 1.0;   // normalised
};

// Ray-tracing render settings
struct RenderParams {
    int    width          = 1280;
    int    height         = 720;
    int    max_steps      = 1000;
    double step_size      = 0.05;
    double abort_distance = 100.0;
};

class Config {
public:
    Config() = default;
    void loadFromFile(const std::string& path);

    BlackholeParams blackhole;
    CameraParams    camera;
    DiskParams      disk;
    RenderParams    render;
    int             logger_level = 1;  // 0=DEBUG 1=INFO 2=WARN 3=ERROR

private:
    void parseConfig(const json& j);
};

#endif // CONFIG_H
