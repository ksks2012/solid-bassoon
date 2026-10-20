#include "app/headless.h"

#include "app/config.h"
#include "app/render_params.h"
#include "app/window.h"
#include "core/probe_cases.h"
#include "logging/logger.h"
#include "rendering/ray_tracer.h"

#include <GL/glew.h>

#include <cstdio>
#include <fstream>
#include <memory>
#include <vector>

namespace {

// Write the shape sidecar next to the raw buffer so a consumer can never
// silently interpret the bytes with the wrong dimensions.
bool writeSidecar(const std::string& path, int width, int height) {
    std::ofstream meta(path);
    if (!meta.is_open()) {
        return false;
    }
    meta << "width=" << width << "\n"
         << "height=" << height << "\n"
         << "channels=4\n"
         << "dtype=float32\n"
         << "layout=RGBA\n"
         << "byte_order=little\n"
         << "origin=bottom-left\n";
    return meta.good();
}

bool writeRawFloats(const std::string& path, const std::vector<float>& data) {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        return false;
    }
    out.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size() * sizeof(float)));
    return out.good();
}

bool hasComputeContext() {
    GLint major = 0;
    GLint minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    bool versionOk = (major > 4) || (major == 4 && minor >= 3);
    return versionOk && GLEW_ARB_compute_shader;
}

}  // namespace

int runHeadless(const std::string& configPath, const std::string& outPath) {
    // ── Config ────────────────────────────────────────────────────────────────
    Config config;
    try {
        config.loadFromFile(configPath);
    } catch (const ConfigError& e) {
        LOG_ERROR("Headless", std::string("Config error: ") + e.what());
        return headless::kFail;
    }
    const int width  = config.render.width;
    const int height = config.render.height;

    // ── Hidden window + GL context ────────────────────────────────────────────
    std::unique_ptr<Window> window;
    try {
        window = std::make_unique<Window>("Blackhole Renderer (headless)",
                                          width, height, /*visible=*/false);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "SKIPPED: no OpenGL context available (%s)\n", e.what());
        return headless::kSkip;
    }

    if (!hasComputeContext()) {
        std::fprintf(stderr,
                     "SKIPPED: OpenGL 4.3 compute shaders unavailable on '%s'\n",
                     reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
        return headless::kSkip;
    }

    // ── Render one frame ──────────────────────────────────────────────────────
    RayTracer rayTracer;
    if (!rayTracer.init(width, height)) {
        LOG_ERROR("Headless", "Failed to initialise GPU ray tracer");
        return headless::kFail;
    }

    rayTracer.render(buildRayTracerParams(config));
    glFinish();  // all compute writes retired before we read the texture

    std::vector<float> pixels;
    if (!rayTracer.readback(pixels)) {
        LOG_ERROR("Headless", "Output texture readback failed");
        return headless::kFail;
    }

    // ── Write the observation ─────────────────────────────────────────────────
    if (!writeRawFloats(outPath, pixels) || !writeSidecar(outPath + ".meta", width, height)) {
        LOG_ERROR("Headless", "Failed to write output: " + outPath);
        return headless::kFail;
    }

    LOG_INFO("Headless", "Rendered " + std::to_string(width) + "x" +
             std::to_string(height) + " scene to " + outPath);
    std::printf("RENDERED %dx%d -> %s\n", width, height, outPath.c_str());
    return headless::kOk;
}

int runProbe(const std::string& outPath) {
    // ── Hidden window + GL context ────────────────────────────────────────────
    // Nothing is rendered: the probe only needs somewhere to own a compute
    // context, so the window stays tiny and invisible.
    std::unique_ptr<Window> window;
    try {
        window = std::make_unique<Window>("Blackhole Renderer (probe)",
                                          64, 64, /*visible=*/false);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "SKIPPED: no OpenGL context available (%s)\n", e.what());
        return headless::kSkip;
    }

    if (!hasComputeContext()) {
        std::fprintf(stderr,
                     "SKIPPED: OpenGL 4.3 compute shaders unavailable on '%s'\n",
                     reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
        return headless::kSkip;
    }

    // ── Dispatch the shared probe table ───────────────────────────────────────
    RayTracer rayTracer;
    std::vector<float> results;
    if (!rayTracer.runProbe(probe::packInputs(),
                            static_cast<int>(probe::kCases.size()), results)) {
        LOG_ERROR("Probe", "Probe dispatch failed");
        return headless::kFail;
    }

    // ── Write the observation ─────────────────────────────────────────────────
    // Same writer as the CPU reference (probe::writeProbeFile), so the two
    // files cannot drift apart structurally.
    if (!probe::writeProbeFile(outPath, results,
                               static_cast<int>(probe::kCases.size()), "gpu")) {
        LOG_ERROR("Probe", "Failed to write output: " + outPath);
        return headless::kFail;
    }

    LOG_INFO("Probe", "Probed " + std::to_string(probe::kCases.size()) +
                          " cases to " + outPath);
    std::printf("PROBED %zu cases -> %s\n", probe::kCases.size(), outPath.c_str());
    return headless::kOk;
}
