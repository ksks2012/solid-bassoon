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
    }

    // Logger
    if (j.contains("logger")) {
        auto& l = j["logger"];
        if (l.contains("level")) logger_level = l["level"].get<int>();
    }
}
