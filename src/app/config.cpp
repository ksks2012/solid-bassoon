#include "app/config.h"
#include "logging/logger.h"
#include <fstream>

void Config::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw ConfigError("Failed to open config file: " + path);
    }

    json j;
    try {
        file >> j;
    } catch (const json::parse_error& e) {
        throw ConfigError(std::string("JSON parse error: ") + e.what());
    }

    parseConfig(j);
    LOG_INFO("Config", "Loaded configuration from " + path);
}

void Config::parseConfig(const json& j) {
    // Blackhole
    if (j.contains("blackhole")) {
        auto& bh = j["blackhole"];
        if (bh.contains("mass"))  blackhole.mass = bh["mass"].get<double>();
        if (bh.contains("spin"))  blackhole.spin = bh["spin"].get<double>();
        if (bh.contains("rs"))    blackhole.rs   = bh["rs"].get<double>();
    }

    // Camera
    if (j.contains("camera")) {
        auto& cam = j["camera"];
        if (cam.contains("distance"))    camera.distance    = cam["distance"].get<double>();
        if (cam.contains("inclination")) camera.inclination = cam["inclination"].get<double>();
        if (cam.contains("azimuth"))     camera.azimuth     = cam["azimuth"].get<double>();
        if (cam.contains("fov"))         camera.fov         = cam["fov"].get<double>();
    }

    // Accretion disk
    if (j.contains("accretion_disk")) {
        auto& d = j["accretion_disk"];
        if (d.contains("inner_radius")) disk.inner_radius = d["inner_radius"].get<double>();
        if (d.contains("outer_radius")) disk.outer_radius = d["outer_radius"].get<double>();
        if (d.contains("temperature"))  disk.temperature  = d["temperature"].get<double>();
    }

    // Rendering
    if (j.contains("rendering")) {
        auto& r = j["rendering"];
        if (r.contains("width"))          render.width          = r["width"].get<int>();
        if (r.contains("height"))         render.height         = r["height"].get<int>();
        if (r.contains("max_steps"))      render.max_steps      = r["max_steps"].get<int>();
        if (r.contains("step_size"))      render.step_size      = r["step_size"].get<double>();
        if (r.contains("abort_distance")) render.abort_distance = r["abort_distance"].get<double>();
        if (r.contains("exposure"))       render.exposure       = r["exposure"].get<double>();
        if (r.contains("gamma"))          render.gamma          = r["gamma"].get<double>();
    }

    // Relativistic jet
    if (j.contains("relativistic_jet")) {
        auto& jt = j["relativistic_jet"];
        if (jt.contains("enabled"))          jet.enabled          = jt["enabled"].get<bool>();
        if (jt.contains("base_radius"))      jet.base_radius      = jt["base_radius"].get<double>();
        if (jt.contains("opening_angle"))    jet.opening_angle    = jt["opening_angle"].get<double>();
        if (jt.contains("profile_exponent")) jet.profile_exponent = jt["profile_exponent"].get<double>();
        if (jt.contains("max_height"))       jet.max_height       = jt["max_height"].get<double>();
        if (jt.contains("radial_sigma"))     jet.radial_sigma     = jt["radial_sigma"].get<double>();
        if (jt.contains("z_falloff"))        jet.z_falloff        = jt["z_falloff"].get<double>();
        if (jt.contains("axial_falloff"))    jet.axial_falloff    = jt["axial_falloff"].get<double>();
        if (jt.contains("bulk_velocity"))    jet.bulk_velocity    = jt["bulk_velocity"].get<double>();
        if (jt.contains("spectral_index"))   jet.spectral_index   = jt["spectral_index"].get<double>();
        if (jt.contains("emissivity"))       jet.emissivity       = jt["emissivity"].get<double>();
    }

    // Logger
    if (j.contains("logger")) {
        auto& l = j["logger"];
        if (l.contains("level")) logger_level = l["level"].get<int>();
    }
}
