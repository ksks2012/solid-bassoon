#include "check.h"

#include "app/config.h"

#include <cstdio>
#include <fstream>
#include <string>

// Unit tests for Config (ticket 0009): struct defaults, JSON overrides,
// unknown-key tolerance, and the two error paths (missing file, malformed
// JSON). Pure CPU — no GL context.

namespace {

const char* kTmpPath = "test_0009_config.json";

void writeFile(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::trunc);
    f << content;
}

bool loadThrows(const std::string& path) {
    try {
        Config cfg;
        cfg.loadFromFile(path);
    } catch (const ConfigError&) {
        return true;
    }
    return false;
}

} // namespace

int main() {
    // ── Struct defaults (documented in config.h) ─────────────────────────────
    {
        Config cfg;
        CHECK(cfg.blackhole.mass == 1.0);
        CHECK(cfg.blackhole.spin == 0.0);
        CHECK(cfg.blackhole.rs == 2.0);

        CHECK(cfg.camera.distance == 30.0);
        CHECK(cfg.camera.inclination == 80.0);
        CHECK(cfg.camera.azimuth == 0.0);
        CHECK(cfg.camera.fov == 60.0);

        CHECK(cfg.disk.inner_radius == 6.0);
        CHECK(cfg.disk.outer_radius == 12.0);
        CHECK(cfg.disk.temperature == 1.0);
        CHECK(cfg.disk.enabled == true);

        CHECK(cfg.render.width == 1280);
        CHECK(cfg.render.height == 720);
        CHECK(cfg.render.max_steps == 1000);
        CHECK(cfg.render.step_size == 0.05);
        CHECK(cfg.render.abort_distance == 100.0);
        CHECK(cfg.render.exposure == 1.0);
        CHECK(cfg.render.gamma == 1.0);

        CHECK(cfg.jet.enabled == false);
        CHECK(cfg.jet.base_radius == 3.2);
        CHECK(cfg.jet.opening_angle == 7.0);
        CHECK(cfg.jet.profile_exponent == 1.0);
        CHECK(cfg.jet.max_height == 40.0);
        CHECK(cfg.jet.radial_sigma == 0.4);
        CHECK(cfg.jet.z_falloff == 10.0);
        CHECK(cfg.jet.axial_falloff == 1.5);
        CHECK(cfg.jet.bulk_velocity == 0.95);
        CHECK(cfg.jet.spectral_index == 0.7);
        CHECK(cfg.jet.emissivity == 1.0);

        CHECK(cfg.logger_level == 1);
    }

    // ── Every documented key is applied ──────────────────────────────────────
    {
        writeFile(kTmpPath, R"({
            "blackhole":        {"mass": 2.5, "spin": 0.7, "rs": 5.0},
            "camera":           {"distance": 40.0, "inclination": 45.0, "azimuth": 10.0, "fov": 30.0},
            "accretion_disk":   {"inner_radius": 5.0, "outer_radius": 20.0, "temperature": 1.5, "enabled": false},
            "rendering":        {"width": 640, "height": 480, "max_steps": 2000, "step_size": 0.02,
                                 "abort_distance": 200.0, "exposure": 2.0, "gamma": 2.2},
            "relativistic_jet": {"enabled": true, "base_radius": 4.0, "opening_angle": 5.0,
                                 "profile_exponent": 0.5, "max_height": 50.0, "radial_sigma": 0.5,
                                 "z_falloff": 8.0, "axial_falloff": 2.0, "bulk_velocity": 0.9,
                                 "spectral_index": 0.8, "emissivity": 2.0},
            "logger":           {"level": 0}
        })");

        Config cfg;
        cfg.loadFromFile(kTmpPath);

        CHECK(cfg.blackhole.mass == 2.5);
        CHECK(cfg.blackhole.spin == 0.7);
        CHECK(cfg.blackhole.rs == 5.0);

        CHECK(cfg.camera.distance == 40.0);
        CHECK(cfg.camera.inclination == 45.0);
        CHECK(cfg.camera.azimuth == 10.0);
        CHECK(cfg.camera.fov == 30.0);

        CHECK(cfg.disk.inner_radius == 5.0);
        CHECK(cfg.disk.outer_radius == 20.0);
        CHECK(cfg.disk.temperature == 1.5);
        CHECK(cfg.disk.enabled == false);

        CHECK(cfg.render.width == 640);
        CHECK(cfg.render.height == 480);
        CHECK(cfg.render.max_steps == 2000);
        CHECK(cfg.render.step_size == 0.02);
        CHECK(cfg.render.abort_distance == 200.0);
        CHECK(cfg.render.exposure == 2.0);
        CHECK(cfg.render.gamma == 2.2);

        CHECK(cfg.jet.enabled == true);
        CHECK(cfg.jet.base_radius == 4.0);
        CHECK(cfg.jet.opening_angle == 5.0);
        CHECK(cfg.jet.profile_exponent == 0.5);
        CHECK(cfg.jet.max_height == 50.0);
        CHECK(cfg.jet.radial_sigma == 0.5);
        CHECK(cfg.jet.z_falloff == 8.0);
        CHECK(cfg.jet.axial_falloff == 2.0);
        CHECK(cfg.jet.bulk_velocity == 0.9);
        CHECK(cfg.jet.spectral_index == 0.8);
        CHECK(cfg.jet.emissivity == 2.0);

        CHECK(cfg.logger_level == 0);
    }

    // ── Unknown keys/sections are ignored, defaults preserved ────────────────
    {
        writeFile(kTmpPath, R"({
            "unknown_section": {"a": 1},
            "blackhole": {"unknown": 123},
            "rendering": {"width": 800}
        })");

        Config cfg;
        bool ok = true;
        try {
            cfg.loadFromFile(kTmpPath);
        } catch (...) {
            ok = false;
        }
        CHECK(ok);
        CHECK(cfg.blackhole.mass == 1.0);   // untouched default
        CHECK(cfg.render.width == 800);     // known key still applied
        CHECK(cfg.render.height == 720);    // untouched default
        CHECK(cfg.disk.enabled == true);    // untouched default (disk on)
    }

    // ── Missing file raises ConfigError ──────────────────────────────────────
    CHECK(loadThrows("no_such_file_0009.json"));

    // ── Malformed JSON raises ConfigError ────────────────────────────────────
    writeFile(kTmpPath, "{ this is not valid json");
    CHECK(loadThrows(kTmpPath));

    std::remove(kTmpPath);
    return check::report("config");
}
